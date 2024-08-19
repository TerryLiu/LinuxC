#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"
#include "thr_list.h"

static pthread_t tid_list; // 线程列表
static int num_list_entry; // 频道数量
static struct mlib_listentry_st *list_entry; // 频道的描述信息

// 线程要执行的函数
static void *thr_list(void *p) {
  int totalsize;
  // 节目单存储的结构体,它内部包含多个频道.它要发送给所有的监听客户端
  struct msg_list_st *entrylistptr; 
  // 频道描述信息的指针
  struct msg_listentry_st *entryptr;
  int ret;
  int size;

  totalsize = sizeof(chnid_t); // 之后逐步累计
  // 累积频道描述信息的大小
  for (int i = 0; i < num_list_entry; ++i) {
    totalsize += sizeof(struct msg_listentry_st) + strlen(list_entry[i].desc);
  }
  // 分配内存
  entrylistptr = malloc(totalsize);
  if (entrylistptr == NULL) {
    syslog(LOG_ERR, "malloc():%s", strerror(errno));
    exit(1);
  }
  // 设置起始的频道id
  entrylistptr->chnid = LISTCHNID; // 这是列表频道

  // 记录一个频道描述对象的起始指针
  entryptr = entrylistptr->entry;
  syslog(LOG_DEBUG, "nr_list_entn:%d\n", num_list_entry);
  for (int i = 0; i < num_list_entry; ++i) {
    size = sizeof(struct msg_listentry_st) + strlen(list_entry[i].desc);

    // 将频传入线程的频道描述信息写入
    entryptr->chnid = list_entry[i].chnid;
    entryptr->len = htons(size);
    strcpy(entryptr->desc, list_entry[i].desc);
    entryptr = (void *)(((char *)entryptr) + size); // 向后移动entptr
    syslog(LOG_DEBUG, "entryp len:%d\n", entryptr->len);
  }

  while (1) {
    syslog(LOG_INFO, "thr_list sndaddr :%d\n", sndaddr.sin_addr.s_addr);
    ret = sendto(serversd, entrylistptr, totalsize, 0, (void *)&sndaddr,
                 sizeof(sndaddr)); // 频道列表在广播网段每秒发送entrylist
    syslog(LOG_DEBUG, "sent content len:%d\n", entrylistptr->entry->len);
    if (ret < 0) {
      syslog(LOG_WARNING, "sendto(serversd, enlistp...:%s", strerror(errno));
    } else {
      syslog(LOG_DEBUG, "sendto(serversd, enlistp....):success");
    }
    sleep(1);
  }
}

// 创建节目单线程. 第一个参数listptr:频道的描述信息;第二个参数num_ent:代表有几个频道
int thr_list_create(struct mlib_listentry_st *listptr, int num_ent) {
  int err;
  list_entry = listptr; // 频道的描述信息
  num_list_entry = num_ent; // 频道数量
  syslog(LOG_DEBUG, "list content: chnid:%d, desc:%s\n", listptr->chnid,
         listptr->desc);

  // 创建线程. 第一个参数是要创建的线程ID的存储位置，第二个参数是线程属性（这里设置为NULL，表示使用默认属性），
  // 第三个参数是线程执行的函数（thr_list），第四个参数是传递给线程函数的参数（这里设置为NULL）
  err = pthread_create(&tid_list, NULL, thr_list, NULL);
  if (err) {
    syslog(LOG_ERR, "pthread_create():%s", strerror(errno));
    return -1;
  }
  return 0;
}

// 销毁节目单线程
int thr_list_destroy(void) {
  // 尝试取消由tid_list标识的线程。
  pthread_cancel(tid_list);
  // 阻塞等待tid_list线程结束，并回收资源。不保存返回值。
  pthread_join(tid_list, NULL);
  return 0;
}
