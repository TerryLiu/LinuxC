#ifndef CLIENT_H_
#define CLIENT_H_
// mpg123命令的参数中含有"-", 这代表使用标准输入作为数据源
#define DEFAULT_PLAYERCMD " /usr/bin/mpg123 -   > /dev/null"
// #define DEFAULT_PLAYERCMD " /usr/bin/mplayer -   > /dev/null"
struct client_conf_st
{
  char *rcvport; // for local using
  char *mgroup;
  char *player_cmd;
};
// 如果不写extern,会报重定义错误. 这里加上extern,表示此变量在其他源文件中定义。
// 如果有其它文件引用了本h文件, 也就可以看到该全局变量client_conf了.
extern struct clinet_conf_st client_conf;

#endif
