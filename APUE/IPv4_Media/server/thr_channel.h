#ifndef THR_CHANNEL_H_
#define THR_CHANNEL_H_

// 普通的频道

#include "medialib.h"
// 创建频道线程
int thr_channel_create(struct mlib_listentry_st *);
// 销毁频道线程
int thr_channel_destroy(struct mlib_listentry_st *);
// 销毁全部的频道线程
int thr_channel_destroyall(void);

#endif // THR_CHANNEL_H_
