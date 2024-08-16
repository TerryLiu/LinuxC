#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include "mytbf.h"

static int min(int a, int b) { return a < b ? a : b; }
// 真实的流量控制令牌桶的结构体,用来替代头文件中的mytbf_t
struct mytbf_st {
  int cps; // 每秒产生的令牌数量,即单步值
  int burst;  // 桶的最大容量
  int token; // 当前桶内令牌数量
  int pos;  // 可能用于内部状态的位置标识
  pthread_mutex_t mut; // 互斥锁，保证线程安全。
  pthread_cond_t cond; // 条件变量，用于线程间同步。
};
// 声明一个令牌桶数组,用来作为任务队列
static struct mytbf_st *job[MYTBF_MAX];
// 声明了一个名为 mut_job 的静态互斥锁，并使用 PTHREAD_MUTEX_INITIALIZER 宏进行了初始化。
// 这使得 mut_job 在程序启动时即成为一个有效的、未加锁的互斥锁，可以立即用于线程同步。
// mut_job用作任务队列的互斥锁
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;
// 用作 pthread_once() 函数的预定义初始值，确保多个线程尝试调用module_load时,仅被执行一次。
static pthread_once_t once_init = PTHREAD_ONCE_INIT;
static pthread_t tid;

// 定时器信号处理函数,向每个令牌桶里添加cps个令牌
static void alrm_handle(int sig) {
  pthread_mutex_lock(&mut_job);
  // 遍历任务队列
    for (int i = 0; i < MYTBF_MAX; ++i) {
      // 如果这个令牌桶存在
      if (job[i] != NULL) {
        // 就锁住令牌桶的互斥锁
        pthread_mutex_lock(&job[i]->mut);
        // 将令牌数量增加cps
        job[i]->token += job[i]->cps;
        // 如果令牌数量大于桶的最大容量
        if (job[i]->token > job[i]->burst) {
          job[i]->token = job[i]->burst;
        }
        // 唤醒所有等待cond的线程
        pthread_cond_broadcast(&job[i]->cond); // 惊群
        // 解锁令牌桶的互斥锁
        pthread_mutex_unlock(&job[i]->mut);
      }
    }
    // 解锁任务队列的互斥锁
    pthread_mutex_unlock(&mut_job);
}

static void *thr_alrm(void *p) {
 // 初始化一个itimerval结构体用于设置定时器
  struct itimerval tick;
  // 使用0初始化tick结构体
  memset(&tick, 0, sizeof(tick));
  // 设置初始延时为1秒
  tick.it_value.tv_sec = 1;  
  // 设置初始延时的微秒部分为0
  tick.it_value.tv_usec = 0;  
  // 设置定时器的间隔时间为1秒
  tick.it_interval.tv_sec = 1;
  // 设置定时器间隔时间的微秒部分为0
  tick.it_interval.tv_usec = 0;

  // 设置一个时钟信号的处理函数为alrm_handle
  signal(SIGALRM, alrm_handle);
  // 基于系统的真实时间来计时，并在计时结束后发送 SIGALRM 信号给进程。
  setitimer(ITIMER_REAL, &tick, NULL);

  while (1) {
    // 暂停线程执行，等待信号
    pause();
  }
  // 线程永远不会正常结束，因此不会返回
  return NULL;
}

// 模块卸载函数
static void module_unload() {
  int i;
  // 取消由 tid 标识的线程。
  pthread_cancel(tid);
  // 等待一个线程的结束。它会阻塞当前线程，直到目标线程tid完成执行。
  pthread_join(tid, NULL);

  for (int i = 0; i < MYTBF_MAX; i++) {
    free(job[i]);
  }
  return;
}

// 模块加载函数, 创建一个维护时钟信号的线程, 以便其它线程使用该时钟信号
static void module_load() {
  int err;
  /* 
  使用pthread_create函数创建一个新的线程；
&tid是用来存储新线程的标识符的变量的地址；
NULL是指向线程属性对象的指针，此处使用默认属性；
thr_alrm是指向线程将要执行的函数的指针，此处是空函数；
最后一个NULL是指向传递给新线程的函数的参数的指针，此处无参数。 
 */
  err = pthread_create(&tid, NULL, thr_alrm, NULL);
  if (err) {
    fprintf(stderr, "pthread_create():%s", strerror(errno));
    exit(1);
  }
  // 在程序正常退出前，注册一个回调函数module_unload的钩子函数
  atexit(module_unload);
}

// 查找一个空闲的槽位
static int get_free_pos_unlocked() {
  for (int i = 0; i < MYTBF_MAX; ++i) {
    if (job[i] == NULL) {
      return i;
    }
  }
  return -1;
}

// 初始化一个令牌桶.cps=每秒钟要生成多少个令牌,burst=令牌桶的最大容量
mytbf_t *mytbf_init(int cps, int burst) {
  // 声明一个令牌桶的指针
  struct mytbf_st *me;

  module_load();                         // 开启定时token派发
  pthread_once(&once_init, module_load); // 限定只开启一次

  int pos;
  // 初始化一个令牌桶,并分配内存空间.
  // 这里sizeof(*me)计算的是me指向的结构体类型的大小，而不是指针本身的大小。*me表示的是me所指向的数据类型
  me = malloc(sizeof(*me));
  if (me == NULL) {
    return NULL;
  }
  me->cps = cps;
  me->burst = burst;
  me->token = 0;
  pthread_mutex_init(&me->mut, NULL); // 初始化该令牌桶的mutex
  pthread_cond_init(&me->cond, NULL); // 初始化该令牌桶的conditional variable
  pthread_mutex_lock(&mut_job);
  // 查找一个空闲的槽位,并记录到pos中
  pos = get_free_pos_unlocked();
  if (pos < 0) {
    // 如果没有找到空闲的槽位,则先解锁
    pthread_mutex_unlock(&mut_job);
    fprintf(stderr, "no free position,\n");
    // 再释放令牌桶的内存空间
    free(me);
    // 退出程序
    exit(1);
  }
  // 记录下该令牌桶在整个任务队列中的位置
  me->pos = pos;
  job[me->pos] = me; // 分配槽位

  pthread_mutex_unlock(&mut_job);
  return me;
}

/**
 * 获取指定数量的令牌
 * 
 * @param ptr 指向mytbf_t结构体的指针，表示令牌桶对象。
 * @param size 请求的令牌数量。
 * @return 返回实际获取的令牌数量。
 */
int mytbf_fetchtoken(mytbf_t *ptr, int size) {
  int n;
  // 将抽象结构体mytbf_t指针转为实际结构体mytbf_st指针
  struct mytbf_st *me = ptr;
  // 获取令牌桶的互斥锁
  pthread_mutex_lock(&me->mut);
  // 如果令牌数量用完,就等待信号量通知
  while (me->token <= 0)
  /* 在多线程环境下等待条件变量me->cond上的信号量通知。在等待期间，线程会释放互斥锁me->mut，以允许其他线程访问共享资源。
  只有当条件变量被信号量通知时，线程才会被唤醒并重新获取互斥锁继续执行。 */
    pthread_cond_wait(&me->cond, &me->mut); // 没有令牌的时候 等待信号量通知
  // 计算能提供的可用令牌数量
  n = min(me->token, size);
  // 减去可用令牌的数量
  me->token -= n;
  /* 广播条件变量 me->cond，唤醒所有等待该条件变量的线程。
被唤醒的线程需重新获取互斥锁并检查条件，仅符合条件的线程会继续执行。
 */
  pthread_cond_broadcast(&me->cond);
  // 解锁令牌桶的互斥锁
  pthread_mutex_unlock(&me->mut);
  return n;
}
// 还回指定数量的令牌
int mytbf_returntoken(mytbf_t *ptr, int size) {
  struct mytbf_st *me = ptr;
  pthread_mutex_lock(&me->mut);
  // 加上还回来的令牌
  me->token += size;
  // 如果令牌数量超过上限，则令牌数量等于上限
  if (me->token > me->burst)
    me->token = me->burst;
  pthread_cond_broadcast(&me->cond);
  pthread_mutex_unlock(&me->mut);
  return 0;
}

// 销毁一个令牌桶
int mytbf_destroy(mytbf_t *ptr) {
  struct mytbf_st *me = ptr;
  // 对任务队列的互斥锁加锁
  pthread_mutex_lock(&mut_job);
  // 清空任务队列中该令牌桶的位置
  job[me->pos] = NULL;
  // 解锁任务队列的互斥锁
  pthread_mutex_unlock(&mut_job);
  // 销毁该令牌桶的互斥锁和条件变量
  pthread_mutex_destroy(&me->mut);
  pthread_cond_destroy(&me->cond);
  // 释放令牌桶的内存空间
  free(ptr);
  return 0;
}

int mytbf_checktoken(mytbf_t *ptr) {
  int token_left = 0;
  struct mytbf_st *me = ptr;
  pthread_mutex_lock(&me->mut);
  token_left = me->token;
  pthread_mutex_unlock(&me->mut);
  return token_left;
}
