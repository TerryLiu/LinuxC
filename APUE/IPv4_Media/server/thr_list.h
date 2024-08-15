#ifndef THR_LIST_H_
#define THR_LIST_H_

// No.0 列表频道

#include "medialib.h"
// 创建节目单线程
int thr_list_create(struct mlib_listentry_st *, int);
// 销毁节目单线程
int thr_list_destroy();

#endif // THR_LIST_H_
