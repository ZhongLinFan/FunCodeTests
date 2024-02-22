#ifndef _IO_HANDLER_H_
#define _IO_HANDLER_H_
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "common.h"

/*网络io和文件io*/
#define MaxNameLen 128
#define MaxExtraFieldLen 512
#define MaxBufLen  40960

typedef struct{
    uint8_t fileSize : 4;
    uint8_t version : 4;

    uint8_t extraFieldLen : 2;
    uint8_t fileNameLen : 2;
    uint8_t lastModifyTime : 4;
    char name[MaxNameLen];
    char extraField[MaxExtraFieldLen];
}FileInfo;

typedef struct 
{
    unsigned char buf[MaxBufLen];
    int readPos;
    int writePos;
    bool isEnd;
    int bufLen;
    FILE* file;
    FileInfo fileInfo;
}FileHandler;

bool initFileHandler(FileHandler* fileHandler);
bool initSenderFileHandler(FileHandler* fileHandler, const char* name, uint8_t version);  
bool initRecvierFileHandler(FileHandler* fileHandler);  
int readFileToBuf(FileHandler* fileHandler);
int writeBufToFile(FileHandler* fileHandler);
void destroyFileHandler(FileHandler* fileHandler); 

typedef struct 
{
    int lfd;    //客户端没用
    int cfd;
    unsigned char buf[MaxBufLen];
    int readPos;
    int writePos;
    int lastSendBytes;
    unsigned short port;
    char ip[16];
    struct sockaddr_in addr;
}NetHandler;

bool initNetHandler(NetHandler* netHandler, unsigned short port, const char* ip,  int ipSize);
bool initSenderNetHandler(NetHandler* netHandler, unsigned short port, const char* ip,  int ipSize);
bool initRecvierNetHandler(NetHandler* netHandler, unsigned short port, const char* ip,  int ipSize);
int readSocketToBuf(NetHandler* netHandler);
int writeBufToSocket(NetHandler* netHandler, int size, bool isFreshBuf);
//返回可用大小
int adjustNetBuf(NetHandler* netHandler, int targetSize);
int copyDataToNetBuf(NetHandler* netHandler, const char* data, int size);
int copyFileBufToNetBuf(NetHandler* netHandler, FileHandler* fileHandler, int copySize, bool* isAdjustNetBuf);
void destroyNetHandler(NetHandler* netHandler);

#endif