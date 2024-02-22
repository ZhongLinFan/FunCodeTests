#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdint.h>

//传输控制
#define SOH     0x01
#define STX     0x02
#define ETB     0x17
#define ETX     0x03
#define EOT     0x04
#define SYN     0x16
#define DLE     0x10

//应答字符
#define NAK     0x15
#define ACK0    0x1000
#define ACK1    0x1001

//状态机支持转移个数
#define MaxShift    100
//简单日志
#undef DEBUG

#ifdef DEBUG
#define LOG(fmt, args...)	\
	do{\
		printf("%s->line:%d=[", __FUNCTION__, __LINE__);\
		printf(fmt, ##args);\
		printf("]\n");\
	} while (0)
#else
#define LOG(fmt, args...)
#endif

//网络环境
#define FakeNetCondition 0  //模拟网络状态，值在0-100范围内，值越大，越容易出现错误包

typedef enum {
    RecvWait        = 0,
    RecvConnected   = 1,
    RecvedSYN       = 2,
    BLKRecv         = 3,
    BLKDrop         = 4,
    RecvEnd         = 5
}RecvState;

typedef enum {
    ConnectedWait   = 6,
    SendConnected   = 7,
    SendWait        = 8,
    SYN_BCC_Send    = 9,
    BLKOK           = 10,
    BLKERR          = 12,
    LastBLK         = 13,
    SendEnd         = 14
}SendState;

typedef enum {
    SenderConnect    = 0x01,
    RecvierACK       = 0x02,
    RecvierNAK       = 0x04,
    SenderLastBLK    = 0x08,
    SenderSYN        = 0x10
}Event;

#endif  //_COMMON_H_



