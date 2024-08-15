#ifndef SERVER_CONF_H_
#define SERVER_CONF_H_
// 设置默认的媒体库位置
#define DEFAULT_MEDIADIR "../medialib"
// 设置默认的网卡名称
#define DEFAULT_IF "enp4s0f3u2u2"

enum
{
  RUN_DAEMON = 1, // 运行在后台
  RUN_FOREGROUND  // 运行在前台
};

struct server_conf_st
{
  char *rcvport; // 接收端口
  char *mgroup; // 多播组
  char *media_dir; // 媒体目录
  char runmode; // 运行模式
  char *ifname; // 网卡名称
};
// 说明外部可访问的变量
extern struct server_conf_st server_conf;
extern int serversd;
extern struct sockaddr_in sndaddr;

#endif // SERVER_CONF_H_
