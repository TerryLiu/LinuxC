#ifndef MEDIALIB_H_
#define MEDIALIB_H_

#include "../include/proto.h"
#include <stdio.h>

// 记录每一条节目单信息 频道号 描述信息
struct mlib_listentry_st{
  chnid_t chnid;
  char *desc;
};
// 获取节目单
int mlib_getchnlist(struct mlib_listentry_st **mchnarr, int *index);
// 释放节目单
int mlib_freechnlist(struct mlib_listentry_st *mchn);
// 读取频道信息
ssize_t mlib_readchn(chnid_t, void *, size_t);

#endif // MEDIALIB_H_
