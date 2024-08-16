#ifndef MYTBF_H_
#define MYTBF_H_

#define MYTBF_MAX 1024
// 定义一个令牌桶结构体,这是一个用void伪装的抽象数据类型.在c文件中会有实际的结构体.
typedef void mytbf_t;

// 初始化流量控制对象,返回一个mytbf_t结构体指针
mytbf_t *mytbf_init(int cps, int burst);
// 获取一个令牌
int mytbf_fetchtoken(mytbf_t *, int);
// 归还一个令牌
int mytbf_returntoken(mytbf_t *, int);
// 检查令牌状态
int mytbf_checktoken(mytbf_t *);
// 销毁流量控制对象
int mytbf_destroy(mytbf_t *);

#endif // MYTBF_H_
