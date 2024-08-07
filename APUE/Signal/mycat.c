#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
// 令牌桶的三要素
static const int SIZE = 30;
static const int CPS = SIZE; // 最大流速(每秒字节数)
static const int BURST = 100;//最大令牌数

static volatile int token = 0;//持有令牌数

static void handler(int sig){
    alarm(1);
    token++;
    if (token >= BURST){
        token = BURST;
    }
}

int main(int argc,char** argv)
{
    if (argc < 2){
        fprintf(stdout,"Usage...");
        exit(1);
    }
    // 打开传入的文件路径
    int sfd = open(argv[1],O_RDONLY);
    if (sfd < 0){
        strerror(errno);
        exit(1);
    }

    int dfd = 1;
    char buf[SIZE];
    
    // 注册信号处理函数
    signal(SIGALRM,handler);
    // 定义一个1秒钟的signal
    alarm(1);

    while(1){
        int len = 0;//read的返回值
        int ret = 0;//write的返回值
        int pos = 0;//写截至的位置
        // 读取文件数据
        len = read(sfd,buf,CPS);
        while (len < 0){
            if (errno == EINTR)
              continue;
            strerror(errno);
            break;
        }

        //读取结束
        if (len == 0){
            break;
        }

        //以防写入不足
        while(len > 0){
            ret = write(dfd,buf+pos,len);
            while (ret < 0){
                if (errno == EINTR){
                  continue;
                }
                printf("%s\n",strerror(errno));
                exit(1);
            }

            pos += ret;
            len -= ret;
        }
        // 如果没有累计的令牌,就暂停执行,等待信号来打断
        while(token <= 0){
            pause();
        }
        // 执行完了,释放令牌
        token--;
    }

    close(sfd);

    exit(0);
}

