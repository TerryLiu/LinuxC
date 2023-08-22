#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <shadow.h>
#include <string.h>

int main(int argc,char **argv)
{
    char *input_passwd;//来自命令行的密码原文
    char *crypted_passwd;
    struct spwd *shadowline;
    
    if (argc < 2) {
        fprintf(stderr,"Usage...\n");
        exit(1);
    }

    input_passwd = getpass("PassWoed:");

    shadowline = getspnam(argv[1]);
    printf("sp_pwdp:%s \n",shadowline->sp_pwdp);
/**
 * crypt()函数的第二个参数是salt,用于生成不同的密码哈希结果。

对于salt参数:

1. 如果是字符串形式,例如"ab9",那么就是一个随机生成的salt值。

2. 如果传入的是一个已经加密后的密码字符串,例如从shadow文件中读取的sp_pwdp的值,那么crypt()会从这个字符串中提取salt值进行再次加密。

也就是说,crypt()也支持传入已经加密后的字符串作为salt参数,这时它内部会自动提取salt值。

例如:

```c
struct spwd *shadow;
shadow = getspnam("username"); 

char *crypted = crypt("newpass", shadow->sp_pwdp);
```

这里从shadow结构获取已加密的密码字符串作为salt,这样生成的新加密结果crypted可以直接写入shadow替换旧的密码,实现密码修改的目的。

所以crypt()的salt参数不仅可以是随机字符串,也可以是包含salt的已加密密码字符串,来实现密码加密验证的目的。这是crypt()的一个特殊用法。
*/
    crypted_passwd = crypt(input_passwd,shadowline->sp_pwdp);
    
    if (strcmp(shadowline->sp_pwdp,crypted_passwd) == 0)
      puts("ok!");
    else
      puts("failed!");

    return 0;
}

