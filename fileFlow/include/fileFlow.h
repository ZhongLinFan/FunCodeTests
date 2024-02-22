#ifndef _FILE_FLOW_H_
#define _FILE_FLOW_H_
#include "stateShift.h"
#include "ioHandler.h"


typedef struct{
    FileHandler fileHandler;
    StateShifter stateShift;
    NetHandler netHandler;
}FileFlow;

typedef struct 
{
    union
    {
        uint16_t response;
        struct{
            uint16_t syn1;
            uint16_t syn2;
        };
        struct{
            uint8_t etb_x;
            uint8_t bccNet;
            uint8_t bccCheck;
        };
    };
    uint16_t flag;
}Marker;


unsigned short getEvtId(Marker* marker);
void fillMarker(FileFlow* fileFlow, Marker* marker);
//返回通信字节量（包含dle，如果是接收端在用，那么应该等于size）
bool isCtlByte(char byte);
int getBCC(const char* data, int size, uint16_t* bbc);
int getBCCSender(const char* data, int size, uint16_t* bbc);
bool destroyFileFlow(FileFlow* flow);

#endif