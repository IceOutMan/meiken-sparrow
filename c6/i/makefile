CC = gcc
# -I （ 大写的 i ）用于指定依赖文件所在的目录，
# -g 用于指定 gee 包含符号表信息，用于调试。
# -D 用于“自定义”一个宏，
# -a （小写的 L ）用于编译时指定包含一个依赖库，其后的 a 表示数学库 math
# -W、－Wstriet-prototypes 、－Wmissing-prototypes 和”Wsyst巳m-headers 用于指定编译时的报警 级别。
# 调试时使用 -D 打开DEGBUG
#CFLAGS = -g -DDEBUG -lm -Wall -I object -I vm -I compiler -I parser -I include -I cli -I gc -W -Wstrict-prototypes -Wmissing-prot otypes -Wsystem-headers
CFLAGS = -g -lm -Wall -I object -I vm -I compiler -I parser -I include -I cli -I gc -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers
TARGET = spr
DIRS = object include cli compiler parser vm gc
CFILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c, %.o, $(CFILES))

# 目标 : 依赖1 依赖2 ... 依赖n
#		"\t" 命令
$(TARGET):$(OBJS)
			$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

clean:	
	-$(RM) $(TARGET) $(OBJS)
r: clean $(TARGET)

# TARGET1:
# 	echo $(CFILES)
# TARGET2:
# 	echo $(OBJS)