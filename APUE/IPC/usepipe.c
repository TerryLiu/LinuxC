#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <wait.h>
#include <errno.h>

#define BUFSIZE 1024

int main()
{
    int pd[2]; // 声明一个fd的数组空间
    pid_t pid; // 声明一个子进程的pid
    char buf[BUFSIZE];
    // 检查创建匿名管道是否成功
    if (pipe(pd) < 0){
        perror("pipe()");
        exit(1);
    }

    fprintf(stdout,"hello,welcome to MusicPlayer\n");
    // 创建一个子进程
    pid = fork();
    if (pid < 0){
        perror("fork()");
        exit(1);
    }
    // 如果是子进程,就读管道数据
    if(pid == 0){
        close(pd[1]); //关闭写端

        dup2(pd[0],0);//将读端重定向到 标准输入,即用pd[0]来填充到0号文件描述符
        close(pd[0]);

        int fd = open("/dev/null",O_RDWR);//屏蔽其他标准输出
        dup2(fd,1); //用一个无效文件描述符来填充 标准输出
        dup2(fd,2); //用一个无效文件描述符来填充 标准错误
        // mpg123 - 表示从stdin读取
        execl("/bin/mpg123","mpg123","-",NULL);
        perror("execvp()");
        exit(1);
    }else{
        // 如果是父进程,就写管道数据
        close(pd[0]);//关闭读端
        int fd = open("./test.mp3",O_RDONLY);
        int len;

        while(1){
            len = read(fd,buf,BUFSIZE);
            if (len < 0){
                if (errno == EINTR)
                  continue;
                close(pd[1]);
                exit(1);
            }
            if (len == 0){
                break;
            }
            write(pd[1],buf,len); // 向管道写端写入数据
        }
        close(pd[1]);
        wait(NULL);
    }

    exit(0);
}

