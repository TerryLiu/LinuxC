# 将 -I../include/ (包含路径) 和 -Wall (开启所有警告) 添加到编译标志 (CFLAG) 中。
CFLAG+=-I../include/ -Wall
all:client
# client:client.o 定义了目标client依赖于client.o这个对象文件。
client:client.o
# gcc编译命令：
# $^ 表示所有依赖目标（这里是client.o）。
# -o $@ 指定输出文件名（这里是client）。
# $(CFLAGS) 引入预定义的编译选项。
	gcc $^ -o $@ $(CFLAGS)

clean:
# 删除所有生成的目标文件。
	rm -rf *.o client