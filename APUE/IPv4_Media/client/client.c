#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
//#include <proto.h>
#include "../include/proto.h"
#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <net/if.h>
#include <string.h>

/*
-M --mgroup   specify multicast group.指定多播组
-P --port     specify receive port.指定接收端口
-p --player   specify player.指定播放器
-H --help     show help.显示帮助
*/

/* 在 C 语言中，结构体成员前面加点符号（.）是一种特殊的语法，称为指定成员初始化。
这种语法允许你在初始化结构体时直接指定成员的值，而不需要按照成员在结构体中定义的顺序来进行。 */
// 声明一个全局的默认配置结构体
struct client_conf_st client_conf = {.rcvport = DEFAULT_RCVPORT,
                                     .mgroup = DEFAULT_MGROUP,
                                     .player_cmd = DEFAULT_PLAYERCMD};

static void print_help() {
  printf("-P --port   specify receive port\n");
  printf("-M --mgroup specify multicast group\n");
  printf("-p --player specify player \n");
  printf("-H --help   show help\n");
}

#define BUFSIZE 320*1024/8*3

/*write to fd len bytes data
坚持写够多少字节的函数
*/
static int writen(int fd, const void *buf, size_t len) {
  int count = 0;
  int pos = 0;
  while (len > 0) {
    // 循环调用write函数，直到成功写入len字节的数据
    count = write(fd, buf + pos, len);
    if (count < 0) {
      if (errno == EINTR)
        continue;
      perror("write()");
      return -1;
    }
    // 更新剩余的字节数和偏移量
    len -= count;
    pos += count;
  }
  return pos;
}

int main(int argc, char *argv[]) {

  /*
  initializing
  level:default < configuration file < environment < arg
  */
  int index = 0;
  int sd = 0;
  struct ip_mreqn mreq;     // group setting 存储多播组设置
  struct sockaddr_in laddr; // local address 存储本地地址
  uint64_t receive_buf_size = BUFSIZE; // 用于指定接收缓冲区的大小
  // 在常见的用途中，这样的数组常用于存储管道（pipe）的两个文件描述符，其中一个用于写入数据，另一个用于读取数据。
  int pd[2];  
  pid_t pid;
  struct sockaddr_in server_addr; // 存储服务器地址
  socklen_t serveraddr_len;
  int len;
  int chosenid;
  int ret = 0;
  struct msg_channel_st *msg_channel;
  struct sockaddr_in raddr;
  socklen_t raddr_len;

  // 一个长选项数组
  struct option argarr[] = {{"port", 1, NULL, 'P'},
                            {"mgroup", 1, NULL, 'M'},
                            {"player", 1, NULL, 'p'},
                            {"help", 0, NULL, 'H'},
                            {NULL, 0, NULL, 0}};
  int c;
  while (1) {
    /*long format argument parse*/
    /* 这段C代码使用 getopt_long 函数从命令行参数 argc 和 argv 中解析长选项和短选项。
    它处理以下短选项："P", "M", "p", 和 "H"，以及一个长选项数组 argarr。
    &index 用于跟踪匹配到的长选项的索引。返回值赋给 c，代表当前解析到的选项字符。 */
    c = getopt_long(argc, argv, "P:M:p:H", argarr, &index);
    if (c < 0)
      break;
    switch (c) {
    case 'P':
      client_conf.rcvport = optarg;
      break;
    case 'M':
      client_conf.mgroup = optarg;
      break;
    case 'p':
      client_conf.player_cmd = optarg;
      break;
    case 'H':
      print_help();
      exit(0);
      break;
    default:
    // abort() 函数用于立即终止程序，并发送一个 SIGABRT 信号，通常会产生核心转储文件以供调试。 
      abort();
      break;
    }
  }

  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0) {
    perror("socket()");
    exit(0);
  }
  // multicast group
  // 将点分十进制的IP地址字符串转换为一个32位的整数.即实现了多播组的赋值.
  inet_pton(AF_INET, client_conf.mgroup,
            &mreq.imr_multiaddr); // 255.255.255.255-->0xFF..
  // local address(self)
  // 实现本地接口的 IP 地址的赋值.
  inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
  // local net card
  // if_nametoindex函数是将网卡名称转换为一个整数(即网络设备的索引号)，即实现了网卡编号的赋值.
  mreq.imr_ifindex = if_nametoindex("enp4s0f3u2u2");
  /* setsockopt 函数的第二个参数 level 指定了要设置的选项所属的协议层级或协议栈的特定部分。
  这个参数决定了选项 optname 的解释范围和上下文。
level 参数的一些常见值包括：
SOL_SOCKET：表示选项适用于套接字本身的层次。
IPPROTO_IP 或 SOL_IP：表示选项适用于 IP 层。
IPPROTO_TCP 或 SOL_TCP：表示选项适用于 TCP 协议层。
IPPROTO_UDP 或 SOL_UDP：表示选项适用于 UDP 协议层。 */
/* 
setsockopt函数的参数说明:
sd 是套接字描述符。
IPPROTO_IP 指定了 IP 层。
IP_ADD_MEMBERSHIP 是一个特定于 IP 层的选项，用于让主机加入一个多播组。
&mreq 是指向结构体 ip_mreq 的指针，它包含了多播组地址和接口信息。
sizeof(mreq) 是选项值的大小。
 */
  if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    perror("setsockopt()");
    exit(1);
  }
  // improve efficiency
  /* 
  setsockopt函数将IP_MULTICAST_LOOP选项设为receive_buf_size的值。
  若调用失败，则输出错误信息并退出程序。实际上，并非设置接收缓冲大小，而是控制多播数据是否在本机循环回送。
   */
  if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &(receive_buf_size), sizeof(receive_buf_size)) < 0) {
    perror("setsockopt()");
    exit(1);
  }
  laddr.sin_family = AF_INET;
  // 将接收数据的端口赋值为命令行参数中的端口号
  // htons函数用来将本机字节序转换为网络字节序。
  laddr.sin_port = htons(atoi(client_conf.rcvport));
  // 将本地要绑定的地址设置为0.0.0.0
  inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);
  // 绑定套接字到指定的端口
  if (bind(sd, (void *)&laddr, sizeof(laddr)) < 0) {
    perror("bind()");
    exit(1);
  }
  // 创建了一个匿名管道
  // 参数 pd 是一个包含两个元素的整型数组。pd[0] 代表管道的读端，pd[1] 代表写端。
  if (pipe(pd) < 0) {
    perror("pipe()");
    exit(1);
  }
  // 创建一个子进程
  pid = fork();
  if (pid < 0) {
    perror("fork()");
    exit(1);
  }
  // 子进程的动作: 从管道获取数据, 并进行播放
  if (pid == 0) // child, read, close write
  {
    /*decode*/
    /*mpg123 read from stdin*/
    close(sd);      // socket 在子进程中关闭套接字的文件描述符,因为它不会使用这个socket
    close(pd[1]);   // 0:read, 1:write 还要关闭匿名管道的写端
    dup2(pd[0], 0); // set pd[0] as stdin 将pd[0]重定向为标准输入
    if (pd[0] > 0)  // 如果pd[0]大于0，就代表它没有成为标准输入,那么就关闭pd[0]
      close(pd[0]);
    
    // execl 会替换当前进程映像，执行成功则不返回。
    /* use shell to parse DEFAULT_PLAYERCMD, NULL means to end */
    execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
    // 只有在execl函数执行失败时才会执行下面的代码
    perror("execl()");
    exit(1);
  } 
  else // parent
  // 父进程的动作: 从网络接收数据, 发送给子进程
  {
    /* receive data from network, write it to pipe */
    // receive programme 收节目单
    struct msg_list_st *msg_list;
    msg_list = malloc(MSG_LIST_MAX);
    if (msg_list == NULL) {
      perror("malloc");
      exit(1);
    }
    // 运行时必须要初始化, 否则第一次打印出来的地址会出不正确
    serveraddr_len = sizeof(server_addr);
    //必须从节目单开始
    while (1) {
      // 从UDP 套接字sd接收数据,最多MSG_LIST_MAX字节的数据到msg_list缓冲区中，
      // 并获取发送方地址信息存入server_addr，其长度存入serveraddr_len。返回值len表示实际接收的字节数或错误码。
      len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (void *)&server_addr,
                     &serveraddr_len);
      fprintf(stderr, "server_addr:%d\n", server_addr.sin_addr.s_addr);
      if (len < sizeof(struct msg_list_st)) {
        fprintf(stderr, "massage is too short.\n");
        continue;
      }
      if (msg_list->chnid != LISTCHNID) {
        fprintf(stderr, "current chnid:%d.\n", msg_list->chnid);
        fprintf(stderr, "chnid is not match.\n");
        continue;
      }
      break;
    }
    // 打印节目单,并选择频道
    // printf programme, select channel
    /*
    1.music xxx
    2.radio xxx
    3.....
    */
    // receive channel package, send it to child process
    struct msg_listentry_st *pos;
    // 通过移动节目实体的位置指针,来遍历节目单
    for (pos = msg_list->entry; 
          (char *)pos < ((char *)msg_list + len);
           pos = (void *)((char *)pos) + ntohs(pos->len)) {

      printf("channel:%d%s", pos->chnid, pos->desc);
    }
    /* 释放节目单指针 */
    free(msg_list);
    while (ret < 1) {
      ret = scanf("%d", &chosenid);
      // 如果采集到的数据不是1位数字,就退出程序
      if (ret != 1)
        exit(1);
    }
    // 创建一个用于发送频道数据的结构体
    msg_channel = malloc(MSG_CHANNEL_MAX);
    if (msg_channel == NULL) {
      perror("malloc");
      exit(1);
    }
    // 运行时必须要初始化, 否则第一次打印出来的地址会出不正确
    raddr_len = sizeof(raddr);
    char ipstr_raddr[30];
    char ipstr_server_addr[30];
    char rcvbuf[BUFSIZE];
    uint32_t offset = 0;
    memset(rcvbuf, 0, BUFSIZE);
    int bufct = 0; // buffer count
    while (1) {
      // 从UDP 套接字sd接收数据,最多MSG_CHANNEL_MAX字节的数据到msg_channel缓冲区中
      len = recvfrom(sd, msg_channel, MSG_CHANNEL_MAX, 0, (void *)&raddr, &raddr_len);
      //防止有人恶意发送不相关的包
      // 如果收到的地址和端口与节目单的发送方地址和端口不匹配,就退出程序
      if (raddr.sin_addr.s_addr != server_addr.sin_addr.s_addr) {
        inet_ntop(AF_INET, &raddr.sin_addr.s_addr, ipstr_raddr, 30);
        inet_ntop(AF_INET, &server_addr.sin_addr.s_addr, ipstr_server_addr, 30);
        fprintf(stderr, "Ignore:addr not match. raddr:%s server_addr:%s.\n",
                ipstr_raddr, ipstr_server_addr);
        continue;
      }
      if (raddr.sin_port != server_addr.sin_port) {
        fprintf(stderr, "Ignore:port not match.\n");
        continue;
      }
      if (len < sizeof(struct msg_channel_st)) {
        fprintf(stderr, "Ignore:massage too short.\n");
        continue;
      }
      // 如果收到的频道号与选择的频道号匹配
      if (msg_channel->chnid == chosenid) {
        // 将msg_channel->data中的数据复制到rcvbuf + offset指向的内存空间中，
        // 复制的数据长度为len - sizeof(chnid_t)字节。
        memcpy(rcvbuf + offset, msg_channel->data, len - sizeof(chnid_t));
        offset += len - sizeof(chnid_t);
        // 如果缓冲区计数bufct是偶数，就调用writen函数将rcvbuf中的数据写入管道的写端
        // 这是为了让子进程可以播放的数据不至于太少,出现播放卡顿.
        if (bufct++ % 2 == 0) {
          if (writen(pd[1], rcvbuf, offset) < 0) {
            exit(1);
          }
          offset = 0;
        }
      }

      //可以做一个缓冲机制，停顿1  2 秒，不采用接收一点播放一点
      // if (msg_channel->chnid == chosenid) {
      //  if (writen(pd[1], msg_channel->data, len - sizeof(chnid_t)) < 0) {
      //    exit(1);
      //  }
      //}
    }

    free(msg_channel);
    close(sd);
    exit(0);
  }
}
