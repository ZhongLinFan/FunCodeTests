#include "fileFlow.h"
#include <stdint.h>
#include <string.h>

unsigned short getEvtId(Marker* marker){
    switch (marker->flag)
        {
            case 1:
                return marker->response == ACK0 || marker->response == ACK1 ? RecvierACK : RecvierNAK;       //receiver->sender，必须是肯定的判断，而不能是marker->response == NAK，因为可能传递其他东西
            case 2:
                return marker->syn1 == SYN && marker->syn2 == SYN ? RecvierACK : RecvierNAK;  //sender
            case 3:
                {
                    uint16_t ret = 0;
                    if(marker->etb_x == ETX){
                        ret = SenderLastBLK;
                    }
                    ret |= marker->bccNet == marker->bccCheck ? RecvierACK : RecvierNAK;
                    return ret;
                    break;
                }
            default:
                break;
        }
}

void fillMarker(FileFlow* fileFlow, Marker* marker){
    int bodyStart = 0;
    unsigned char* buf = fileFlow->netHandler.buf;
    int writePos = fileFlow->netHandler.writePos;
    int readPos = fileFlow->netHandler.readPos;
    switch (fileFlow->stateShift.state)
    {
    case RecvConnected:     //只有接收方会收到syn的帧
        marker->flag = 2;
        marker->syn1 = buf[writePos-2];
        marker->syn2 = buf[writePos-1];
        break;
    case RecvedSYN: //receiver后续都是收到块，也就是需要填充flag为3的
        //需要找到ftd开始的位置（可能有header）
        while(bodyStart < writePos && buf[bodyStart] != STX){
            bodyStart++;
        }
        bodyStart++;
    case BLKRecv: 
    case BLKDrop: 
        marker->flag = 3;
        marker->etb_x = buf[writePos-2];
        marker->bccNet = buf[writePos-1];
        uint16_t bbc = 0;
        getBCC(buf+bodyStart, writePos-2-bodyStart, &bbc);
        marker->bccCheck = bbc;

#ifdef DEBUG
        LOG("bodyStart:%d", bodyStart);
        LOG("bodyEnd:%d", writePos-2);
        int count = 0;
        for(int i = readPos; i < writePos; i++){
            printf("%02x ", buf[i]);
            count++;
        }
        LOG("字节数量：%d", count);
        LOG("etb_x:%02x,pos:%d, bccNet:%02x, pos:%d, bbcCheck:%02x", marker->etb_x,writePos-2,  marker->bccNet, writePos-1, marker->bccCheck);
#endif
        break;
    //case SYN_BCC_Send:
    //case SendConnected:  这两个是主动的
    case SYN_BCC_Send:
    case BLKOK:
    case BLKERR:
    case LastBLK:
        {
            marker->flag = 1;
            uint16_t temp = 0;
            int size = sizeof(uint16_t);
            //这里必须使用buf+writePos-2的形式去读取，因为上次的buf布局为|上次发送的数据|ack/nak|，这个时候readPos是没有更新的，因为如果是nak可以方便重复发送了，所以这里不能更新readPos，需要等待事件确定之后再决定是否更新readPos
            LOG("当前要读取的起始位置：%d", writePos-2);
            memcpy((void*)&temp, buf+writePos-2, size);
            //fileFlow->netHandler.readPos += size;
            marker->response = ntohs(temp);
            LOG("收到的ack:%02x", marker->response);
            break;
        }
    default:
        marker->flag = 0;
        break;
    }

}

bool isCtlByte(char byte){
    if(byte == SOH || byte == STX || byte == ETB || byte == ETX || byte == EOT || byte == SYN || byte == DLE){
        return true;
    }
    return false;
}

int getBCC(const char* data, int size, uint16_t* bbc){
    //size = size < 1024 ? size : 1024; //这里不用判断，接收端可能一起判断
    uint16_t sum = 0;
    int i = 0;
    for(; i < size ; i++){
        if(!isCtlByte(data[i])){
            sum += data[i];
        } 
    }
    *bbc = sum;
    return size;
}

int getBCCSender(const char* data, int size, uint16_t* bbc){
    size = size < 1024 ? size : 1024;
    uint16_t sum = 0;
    int sendBytes = getBCC(data, size - 1, &sum);  //这里减去1
    LOG("1sendBytes:%d", sendBytes);
    bool isSendLastByte = true;
    //判断最后一位是否是DLE
    if(data[size-1] == DLE){
        int DLECount = 0;
        int i = size - 2; //从截止位置的前一位开始往前遍历
        while(i >= 0 && data[i] == DLE){
            DLECount++;
            i--;
        }
        //如果为0，说明需要和下一位结合
        if(DLECount % 2 == 0){
            isSendLastByte = false;
        }
        //否则发送最后一位
        LOG("DELCount:%d,是否发送最后一位：%d", DLECount, isSendLastByte);
    }
    //是否发送最后一位
    if (isSendLastByte)
    {
       sendBytes += 1;
        if(!isCtlByte(data[size-1])){
            sum += data[size-1];
        } 
    }
    
    LOG("2sendBytes:%d", sendBytes);
    *bbc = sum;
    return sendBytes;
}

bool destroyFileFlow(FileFlow* flow){
    //调用三个成员的销毁函数
    destroyFileHandler(&flow->fileHandler);
    destroyNetHandler(&flow->netHandler);
    destroyShifter(&flow->stateShift);
}