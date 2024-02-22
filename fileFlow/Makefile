CXX=gcc
CFLAGS=-g -std=gnu99

RECEIVER=./bin/receiver
SENDER=./bin/sender

INC=-I./include
SRC=./src
RECE_SRC=$(filter-out $(SRC)/sender.c, $(wildcard *.c $(SRC)/*.c))
SEND_SRC=$(filter-out $(SRC)/receiver.c, $(wildcard *.c $(SRC)/*.c))

RECE_OBJS=$(patsubst %.c, %.o, $(RECE_SRC))
SEND_OBJS=$(patsubst %.c, %.o, $(SEND_SRC))


receiver:$(RECE_OBJS)
	mkdir -p ./bin
	$(CXX) $(CFLAGS) $(RECE_OBJS) -o $(RECEIVER) $(INC)

sender:$(SEND_OBJS)
	mkdir -p ./bin
	$(CXX) $(CFLAGS) $(SEND_OBJS) -o $(SENDER) $(INC)

%.o:%.c
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

.PHONY:clean

clean:
	rm -rf ./src/*.o $(RECEIVER) $(SENDER)
