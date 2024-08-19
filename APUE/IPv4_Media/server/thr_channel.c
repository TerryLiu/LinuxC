#include <asm-generic/errno-base.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "thr_channel.h"
#include "medialib.h"
#include "server_conf.h"
#include "../include/proto.h"

static int tid_nextpos = 0;

// 每一个线程负责一个频道 
struct thr_channel_entry_st {
  chnid_t chnid; //  频道号
  pthread_t tid; // 处理该频道的线程
};

struct thr_channel_entry_st thr_channel[CHANNUM];

// 组织当前线程要发送的频道数据,并发送出去
static void *thr_channel_snder(void *ptr)
{
  struct msg_channel_st *sbufp; // 要发送的数据
  int len;
  struct mlib_listentry_st *entry = ptr; // 频道内容
  sbufp = malloc(MSG_CHANNEL_MAX);
  if (sbufp == NULL) {
    syslog(LOG_ERR, "malloc():%s", strerror(errno));
    exit(1);
  }

  sbufp->chnid = entry->chnid; // 频道号处理
  // 频道内容读取
  while(1) {
    // 读取频道内容,并存入sbufp->data 
    len = mlib_readchn(entry->chnid, sbufp->data, 320*1024/8); // 320kbit/s
    syslog(LOG_DEBUG, "mlib_readchn() len: %d", len);
    if (len < 0) {
      break;
    }
    // 发送数据
    if (sendto(serversd, sbufp, len + sizeof(chnid_t), 0, (void*)&sndaddr, sizeof(sndaddr)) < 0) {
      syslog(LOG_ERR, "thr_channel(%d):sendto():%s", entry->chnid,
             strerror(errno));
      break;
    }
    // 让当前线程自愿放弃处理器；
    // 将线程置于就绪队列末尾；
    sched_yield();
  }
  pthread_exit(NULL);
}

// 创建对应的频道线程
int thr_channel_create(struct mlib_listentry_st *ptr) {
  int err;
  err = pthread_create(&thr_channel[tid_nextpos].tid, NULL, thr_channel_snder, ptr);
  if (err) {
    // strerror将错误码转换为可读的字符串
    syslog(LOG_WARNING, "pthread_create():%s", strerror(err));
    return -err;
  }
  thr_channel[tid_nextpos].chnid = ptr->chnid; //填写频道id
  tid_nextpos++;
  return 0;
}

// 销毁对应的频道线程
int thr_channel_destroy(struct mlib_listentry_st *ptr) {
  for (int i = 0;i < CHANNUM;++i) {
    if (thr_channel[i].chnid == ptr->chnid) {
      if (pthread_cancel(thr_channel[i].tid) < 0) {
        syslog(LOG_ERR, "pthread_cancel():thr thread of channel%d", ptr->chnid);
        return -ESRCH;
      }
      pthread_join(thr_channel[i].tid, NULL);
      thr_channel[i].chnid = -1;
      break;
    }
  }
  return 0;
}

// 销毁所有的频道线程
int thr_channel_destroyall(void) {
  for (int i = 0; i < CHANNUM; i++) {
    if (thr_channel[i].chnid > 0) {
      if (pthread_cancel(thr_channel[i].tid) < 0) {
        syslog(LOG_ERR, "pthread_cancel():thr thread of channel:%ld",
               thr_channel[i].tid);
        return -ESRCH;
      }
      pthread_join(thr_channel[i].tid, NULL);
      thr_channel[i].chnid = -1;
    }
  }
  return 0;
}
