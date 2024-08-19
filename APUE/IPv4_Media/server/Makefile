##
# IPv4_Media_Server
#
# @file
# @version 0.1

# 向C编译器选项添加搜索目录../include/以查找头文件。
# CFLAGS+=-pthread：启用对线程的支持。
# CFLAGS+=-Wall：开启所有警告消息。
CFLAGS+=-I../include/ -pthread -Wall
# 链接阶段支持多线程，确保程序可以使用线程库。
LDFLAGS+=-pthread
all:server
# 在Makefile中，$(CC) 是一个变量，通常用来表示编译器的名称或路径。
# 这里它代表用于编译源代码的命令，比如 gcc 或 clang。
# 这行命令的意思是使用 $(CC) 指定的编译器来编译和链接所有的依赖对象
# 文件 $^，生成最终的可执行文件 $@，
# 同时传递编译期标志 $(CFLAGS) 和链接期标志 $(LDFLAGS)。
server:server.o thr_channel.o thr_list.o medialib.o mytbf.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm *.o server -rf

# end
