#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc,char **argv)
{
    if (argc < 2) {
        fprintf(stderr,"Usage...\n");
        exit(1);
    }
    // 用写入方式打开一个文件, 不存在就创建, 存在则清空它
    int fd = open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,0600);
    if (fd < 0) {
        perror("open()");
        exit(1);
    }
    // 向后移动5G-1个字节的空间
    // 如果计算因子不带长整型(LL)标识, 编译时会报警告: 整数溢出
    // 要点: 要将GCC给出的警告调到没有, 因为它其实是一个错误
    long err = lseek(fd,5LL*1024LL*1024LL*1024LL-1LL,SEEK_SET);
    if (err == -1) {
        perror("lseek");
        exit(1);
    }
    // 必须写入一个字符,否则这个文件的写入会是空的
    write(fd,"",1);

    return 0;
}

