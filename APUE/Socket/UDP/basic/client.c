#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "proto.h"

#define SERVERPORT "2333"

int main()
{
    int sfd;
    struct msg_st *sbuf; // 声明一个消息结构体指针,所以后面需要动态初始化
    struct sockaddr_in raddr;//remote addr
    // 获取UDP套接字对象
    sfd = socket(AF_INET,SOCK_DGRAM,0/*IPPROTO_UDP*/);
    // 为消息对象动态分配内存
    int pkglen = sizeof(struct msg_st)+strlen("Mike")+1;// 最后加1是为了给字符串结束符'\0'留位置
    sbuf = malloc(pkglen);
    if (sbuf == NULL){
        perror("malloc()");
        exit(1);
    }
    
    char *name = "Mike";
    strcpy(sbuf->name,name);
    sbuf->math = htonl(rand()%100);//主机字节序转网络字节序
    sbuf->chinese = htonl(rand()%100);

    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(atoi(SERVERPORT));
    // 将IPv4点分式转为大整数
    inet_pton(AF_INET,"127.0.0.1",&raddr.sin_addr);
    // 向指定UDP服务器地址发送数据sbuf
    if(sendto(sfd,sbuf,pkglen,0,(void *)&raddr,sizeof(raddr)) < 0){
        perror("sendto()");
        exit(1);
    }

    puts("OK");

    close(sfd);

    exit(0);
}
