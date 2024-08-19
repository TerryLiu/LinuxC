#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"
#include "thr_channel.h"
#include "thr_list.h"

int serversd;
struct sockaddr_in sndaddr;
struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,
                                     .media_dir = DEFAULT_MEDIADIR,
                                     .runmode = RUN_FOREGROUND,
                                     .ifname = DEFAULT_IF,
                                     .mgroup = DEFAULT_MGROUP};
static struct mlib_listentry_st *list;

static void print_help() {
  printf("-P    指定接收端口\n");
  printf("-M    指定多播组\n");
  printf("-F    前台运行 \n");
  printf("-D    指定媒体库位置\n");
  printf("-I    指定网卡名称\n");
  printf("-H    显示帮助\n");
}
/* 
守护进程的特点:
1. 脱离终端(TTY没有值)
2. 父进程(PPID)是1号
3. 进程号(PID)和group号(PGID)、session号(SID)都相同
 */
// 守护进程退出
static void daemon_exit(int s) {
  thr_list_destroy();
  thr_channel_destroyall();
  mlib_freechnlist(list);
  syslog(LOG_WARNING, "signal-%d caught, exit now.", s);
  closelog();
  exit(0);
}

// 脱离终端
static int daemonize() {
  pid_t pid;
  int fd;
  // 创建一个子进程. 父进程的锁和信号量不会被子进程继承。
  pid = fork();
  if (pid < 0) {
    // perror("fork()");
    syslog(LOG_ERR, "fork() failed:%s", strerror(errno));
    return -1;
  }
  if (pid > 0) // parent
  // 退出父进程
    exit(0);

  // 将标准输入输出和错误文件描述符重定义到一个空设备
  fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    // perror("open()");
    syslog(LOG_WARNING, "open() failed:%s", strerror(errno));
    return -2;
  } else {
    /*close stdin, stdout, stderr*/
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    // 如果空设备描述符大于2, 就说明替换标准输入输出和错误文件描述符失败, 就要关闭它
    if (fd > 2)
      close(fd);
  }
  // 改变工作目录到根目录, 因为/是绝对存在的目录
  chdir("/");
  // 设置进程的文件权限掩码为0。新创建的文件或目录将具有最宽松的权限（无权限被屏蔽）。返回旧的掩码值,这里忽略掉了
  umask(0);
  /* 创建新会话的系统调用。它将当前进程设置为新的会话组组长，并且这个新的会话与任何终端设备都没有关联。
  这个函数通常用于将一个进程从一个控制终端中分离出来，使其成为独立的会话leader，
  这对于后台守护进程等需要脱离终端设备的进程非常有用。
   */
  setsid();
  return 0;
}

// socket 初始化通用步骤
static int socket_init() {
  // 该结构体用于存储多播组的成员信息。它包含三个字段：imr_multiaddr、imr_interface和imr_address。
  // 这些字段分别用于指定多播组的地址、接口地址和源地址。
  struct ip_mreqn mreq;
  inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
  inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);      // local address
  // 获取网卡索引号，并设置到多播组的配置里
  mreq.imr_ifindex = if_nametoindex(server_conf.ifname); // net card
  // 创建一个UDP套接字
  serversd = socket(AF_INET, SOCK_DGRAM, 0);
  if (serversd < 0) {
    syslog(LOG_ERR, "socket():%s", strerror(errno));
    exit(1);
  }
  // 设置多播组
  if (setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) <
      0) {
    syslog(LOG_ERR, "setsockopt(IP_MULTICAST_IF):%s", strerror(errno));
    exit(1);
  }

  // bind()
  // 设置地址家族为 IPv4。
  sndaddr.sin_family = AF_INET;
  // 将服务端口号从字符串转换为整数，并转换成网络字节序。
  sndaddr.sin_port = htons(atoi(server_conf.rcvport));
  // 将多播组地址字符串转换为二进制格式并存储在结构体中。
  inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr.s_addr);
  // inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr);
  // inet_pton(AF_INET, "0.0.0.0", &sndaddr.sin_addr.s_addr);
  

  return 0;
}

int main(int argc, char** argv) {
  int c;
  struct sigaction sa;

  // 设置要执行的处理函数
  sa.sa_handler = daemon_exit;
  // 清空信号处理函数的信号集合
  sigemptyset(&sa.sa_mask);
  // 向信号处理函数的信号集合中添加SIGINT、SIGQUIT、SIGTERM三个信号
  sigaddset(&sa.sa_mask, SIGINT);
  sigaddset(&sa.sa_mask, SIGQUIT);
  sigaddset(&sa.sa_mask, SIGTERM);

  sigaction(SIGTERM, &sa, NULL);  // 常用的打断信号
  sigaction(SIGQUIT, &sa, NULL); // 退出信号
  sigaction(SIGINT, &sa, NULL); // 中断阻断信号

/* openlog 函数配置系统日志服务：
"netradio"：标识产生日志的程序名。
LOG_PID：每条日志包含进程ID。
LOG_PERROR：错误信息输出到标准错误。
LOG_DAEMON：日志级别，表示此日志属于后台进程类别。
 */
  openlog("netradio", LOG_PID | LOG_PERROR, LOG_DAEMON);
#ifdef DEBUG
  fprintf(stdout, "here1!\n");
#endif
  while(1) {
    /* 
    getopt函数用于处理命令行参数，解析选项和参数。其中argc为参数总数，argv为参数数组.
    冒号 (:) 的含义如下：
M: 和 P: 后面跟着冒号，表示这些选项需要一个参数，并且参数必须跟随选项之后，可以紧挨着写（如 -Mvalue），也可以用空格分开（如 -M value）。
F没有冒号，意味着这些选项后面不需要参数。
   */
    c = getopt(argc, argv, "M:P:FD:I:H");
    printf("get command c:%c\n", c);
    if (c < 0) {
      break;
    }
    switch (c) {
    case 'M':
      server_conf.mgroup = optarg;
      break;
    case 'P':
      server_conf.rcvport = optarg;
      break;
    case 'F':
      break;
    case 'D':
      server_conf.media_dir = optarg;
      break;
    case 'I':
      server_conf.ifname = optarg;
      break;
    case 'H':
      print_help();
      exit(0);
      break;
    default:
      abort();
      break;
   }
  }

  if (server_conf.runmode == RUN_DAEMON) { // 后台运行
    if (daemonize() != 0) {
      // 退出前会捕捉信号,然后执行daemon_exit函数
      exit(1);
    }
  } else if (server_conf.runmode == RUN_FOREGROUND) { // 前台运行
    /* do nothing */
  } else {
    syslog(LOG_ERR, "EINVAL server_conf.runmode.");
    exit(1);
  }

  /*SOCKET 初始化*/
  socket_init();
  /*get channel information*/
  int list_size;
  int err;

  // list 频道的描述信息
  // list_size有几个频道
  err = mlib_getchnlist(&list, &list_size);
  if (err) {
    syslog(LOG_ERR, "mlib_getchnlist():%s", strerror(errno));
    exit(1);
  }

  // 创建节目单线程
  thr_list_create(list, list_size);
  /*if error*/

  //创建多个频道线程
  int i = 0;
  for (i = 0; i < list_size; i++) {
    err = thr_channel_create(list + i);
    if (err) {
      fprintf(stderr, "thr_channel_create():%s\n", strerror(err));
      exit(1);
    }
  }
  // 向系统日志记录一条调试信息
  syslog(LOG_DEBUG, "%d channel threads created.", i);
  // 让主进程不要退出
  while (1)
    // 阻塞等待信号,用来节省cpu资源,避免空转
    pause();
}
