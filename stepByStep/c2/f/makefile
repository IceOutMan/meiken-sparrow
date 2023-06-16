CC = gcc
#CFLAGS = -g -DEBUG -lm -Wall -I object -I vm -I compiler -I parser -I include -I cli -I gc -W -Wstrict-prototypes -Wmissing-prot otypes -Wsystem-headers
# -I 依赖文件所在的目录
# -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers 指定报警级别
# -l 编译时指定一个依赖库， lm 表示依赖 math库
# -g 指定 gcc 包含符号表信息，用于调试
CFLAGS = -g -lm -Wall -I vm -I parser -I include -I cli -I -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers
TARGET = spr
# .c 文件在的目录
DIRS =  include cli  parser vm 

# 遍历$(DIRS) -> dir, 每一个dir执行 $(wildcard $(dir)/*.c)
CFILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))

# 目标文件 
OBJS = $(patsubst %.c, %.o, $(CFILES))

# spr : xxx.o xxx.o xxx.o
$(TARGET):$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

clean:
	-$(RM) $(TARGET) $(OBJS)

r: clean $(TARGET)
