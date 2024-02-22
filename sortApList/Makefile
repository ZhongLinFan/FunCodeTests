TARGET=./bin/sortAPList
CXX=gcc
CFLAGS=-g -O2 -std=gnu11

#包含的头文件
INC=-I./include

#目标文件
SRC=$(wildcard ./src/*.c)
OBJS=$(patsubst %.c, %.o, $(SRC))

$(TARGET):$(OBJS)
	mkdir -p bin/
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(INC)  

%.o:%.c
	$(CXX) $(CFLAGS) $< -c -o $@ $(INC) 


.PHONY:clean

clean:
	rm -f $(TARGET) $(SRC)/*.o
