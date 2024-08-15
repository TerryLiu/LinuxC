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
server:server.o thr_channel.o thr_list.o medialib.o mytbf.o
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm *.o server -rf

# end
