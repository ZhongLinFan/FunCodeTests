#include "fileFlow.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct
{
    uint16_t ack;
} ACKState;

static ACKState ackState;

bool initReceiver(FileFlow* receiver, unsigned short port, const char* ip, int ipSize, 
        StateShift* recvStateShift, int stateShiftSize){
    ackState.ack = ACK0;
    initRecvierFileHandler(&receiver->fileHandler);
    initShifter(&receiver->stateShift, RecvWait, recvStateShift, stateShiftSize);
    initRecvierNetHandler(&receiver->netHandler, port, ip, ipSize);
}

int getACK(){
    int state = ackState.ack;
    if(ackState.ack == ACK0){
        ackState.ack = ACK1;
    }else{
        ackState.ack = ACK0;
    }
    return state;
}

int responseData(FileFlow* receiver, const char* data, int size){
    int ret = 0;
    do{
        copyDataToNetBuf(&receiver->netHandler, data, size);
    }while((ret = writeBufToSocket(&receiver->netHandler, size, true)) == -1);
    //这是最恰当的更新时间点，因为刚好一轮结束

    LOG("写入字节数：%d,cfd为：%d", ret, receiver->netHandler.cfd);
    receiver->netHandler.writePos = 0;
    receiver->netHandler.readPos = 0;
}

void responseACK(void* data){
    //在回复时ACK的时候，这次读取的buf的数据肯定没用了，所以可以responseData之后置为0；
    FileFlow* receiver = (FileFlow*)data;
    uint16_t responseCode = getACK();

    uint16_t response = htons(responseCode); //转网络字节序
    LOG("response:%02x", response);

    responseData(receiver, (char*)&response, sizeof(response));
}

void responseNAK(void* data){
    //发现是NAK，说明之前的数据错误，那么丢掉
    FileFlow* receiver = (FileFlow*)data;
    uint16_t responseCode = NAK;
    //丢掉的动作，必须要有，因为如果没有，相当于没有read
    receiver->netHandler.readPos = receiver->netHandler.writePos;

    uint16_t response = htons(responseCode); //转网络字节序
    LOG("response:%02x", response);
    responseData(receiver, (char*)&response, sizeof(response));
}

void recvSYN(void* data){
    FileFlow* receiver = (FileFlow*)data;
    //更新接收到的内容，标记为已读
    receiver->netHandler.readPos += 2;

    responseACK(data);
}

void removeDLE(void* data){
    FileFlow* receiver = (FileFlow*)data;
    //可以直接从readPos那里去除，因为控制字符不可能有DLE，只有通信字符才有DLE

    //检测到DLT要去掉一个，后面一个字节跳过，然后紧凑，但是不需要sender那样，倒着遍历，而是正向遍历即可
    //找到第一个DLE之后的开始节点
    int segStart =  receiver->netHandler.readPos;
    char* buf = receiver->netHandler.buf;
    int segEnd = 0;
    int writePos = receiver->netHandler.writePos - 2; //必须要-2！！！因为BBC完全有可能出现DLE，这出现了一个很坑的bug
    int i = segStart;
    for(; i < writePos && buf[i] != DLE; i++);
    segStart = i+1;
    int moveCount = 0;
    LOG("segStart:%d",segStart);
   //i < writePos这个分支会会跳过while，说明不需要移动，否则至少有一块需要移动
    while(i < writePos){
        //还要在加2。因为buf[i]之后是控制字符，可能是DLE,必须要在for之前+2，为啥+2，
        //因为不管是外面的for还是下面的for在找到DLE之后，i肯定指向DLE，而紧随其后的就是控制字符，所以应该跳过，很容易忽略
        i += 2; 
        for(; i < writePos && buf[i] != DLE; i++);
        segEnd = i; //不需要判断i < writePos && buf[i] != DLE这两种情况，最后一段需要移动，也就是etx和bbc
        LOG("需要移除的字节pos：%d, %02x, segEndPos: %d, segEnd:%d", segStart-1, buf[segStart-1], i, buf[segEnd]);
        memmove(buf + segStart - 1 - moveCount, buf + segStart, segEnd-segStart); //一定要加上moveCount，很容易忽略，因为要迭代上前面的段前移的量
#ifdef DEBUG
        LOG("移动完之后的内存状态：");
        for(int i = 0; i < segEnd; i++){
            printf("%02x ", buf[i]);
        }
#endif
        segStart = segEnd+1;
        moveCount++;
    }
    LOG("移除的DLE数量：%d", moveCount);
    //到这里肯定是i == writePos
    //更新writePos
    receiver->netHandler.writePos -= moveCount;
}

void recvFTD(void* data){
    FileFlow* receiver = (FileFlow*)data;
    char* buf = receiver->netHandler.buf;
    removeDLE(data);
    
    //拷贝到fileInfo
    memcpy(&receiver->fileHandler.fileInfo, buf + receiver->netHandler.readPos + 2, 
        receiver->netHandler.writePos-2-(receiver->netHandler.readPos + 2)); //+2是去掉soh和stx
    LOG("对端文件名称为：%s",receiver->fileHandler.fileInfo.name);

    //初始化文件，并且后续往这个文件里写入
    //先检测当前文件是否存在，如果存在那么重新起一个别名，否则按照解析出来的名称创建
    struct stat st_stat = {0};
    int ret;
    while(!(ret = stat(receiver->fileHandler.fileInfo.name, &st_stat))){
        int size = strlen(receiver->fileHandler.fileInfo.name);
        if (size + 2 < MaxNameLen)
        {
            char name[128] = {0};
            char suffix[16] = {0};
            if(sscanf(receiver->fileHandler.fileInfo.name, "%[^.].%s", name, suffix) == 2){
                sprintf(receiver->fileHandler.fileInfo.name, "%s1.%s", name, suffix);
            }else{
                receiver->fileHandler.fileInfo.name[size] = '1';
                receiver->fileHandler.fileInfo.name[size+1] = '\0';
            }
            LOG("name:%s, suffix:%s", name, suffix);
        }else{
            printf("重名文件过多，无法重新命名，请手动输入：\n");
            scanf("%s", receiver->fileHandler.fileInfo.name);
        }         
    }
    printf("接收文件命名为：%s\n",receiver->fileHandler.fileInfo.name);
    char path[512] = { 0 };
    sprintf(path,"./%s", receiver->fileHandler.fileInfo.name);
    LOG("path:%s", path);
    receiver->fileHandler.file = fopen(path, "w"); //'w'

    //更新读过的readPos
    receiver->netHandler.readPos = receiver->netHandler.writePos;
    responseACK(data);  //在responseACK里回复
}

void recvBlk(void* data){
    FileFlow* receiver = (FileFlow*)data;
    NetHandler* netHandler =  &receiver->netHandler;
    //去除掉多余的DLE
    removeDLE(data);

    //将readPos到writePos-2的数据写到文件中
    int copySize = netHandler->writePos - 2 - netHandler->readPos;
    LOG("当前buf写到文件的起止信息,startPos:%d, endPos:%d", netHandler->readPos, netHandler->writePos - 2);
    fwrite(netHandler->buf + netHandler->readPos, sizeof(char), copySize, receiver->fileHandler.file);
    //TODO可以先写到fileHandler的缓冲区里，然后缓冲区满之后再写到文件里

    //更新读过的readPos
    receiver->netHandler.readPos = receiver->netHandler.writePos; 

    responseACK(data);  //在responseACK里回复
}

void reciveEnd(void* data){
    FileFlow* receiver = (FileFlow*)data;
    //先接收数据
    recvBlk(data);

    //再进行清理工作
    destroyFileFlow(receiver);
    printf("接收完毕，已退出\n");
    exit(0);
}


int main(){
    srand((unsigned)time(NULL));
    FileFlow receiver;
    StateShift recvStateShift[] = {
        { RecvWait, SenderConnect, RecvConnected, NULL, NULL},  //手动开启事件shiftState
        { RecvConnected, RecvierACK, RecvedSYN, recvSYN, &receiver}, //RecvSYN处理header及FTD
        { RecvConnected, RecvierNAK, RecvConnected, responseNAK, &receiver}, //进入到RecvedSYN说明已经接收到同步字段并且正确
        //可以处理header或者ftd，这两个实际中不是一次传输完成的，不过题目中指明了可以一次传输完，RecvierACK这个表示我接收到了FTD，
        //并且校验没问题，才会回复RecvierACK，和发送端的逻辑有点不一样，也就是我是收到socket内容之后，根据这个收取的事件产生对应的事件，被动的执行相应的处理函数，sender会有主动切换的逻辑
        { RecvedSYN, RecvierACK, BLKRecv, recvFTD, &receiver},   
        { RecvedSYN, RecvierNAK, RecvedSYN, responseNAK, &receiver}, //接收到了FTD，但是出错了，所以需要重新发送
        { BLKRecv, RecvierACK, BLKRecv, recvBlk, &receiver},   //收取文件内容并且校验正确，会进行相关存储（这里肯定不会收取FTD相关的内容了）
        { BLKRecv, RecvierNAK, BLKDrop, responseNAK,&receiver},   //收取文件内容出错，丢弃数据
        { BLKDrop, RecvierACK, BLKRecv , recvBlk, &receiver},   //收取文件内容并且校验成功，会进行相关存储
        { BLKDrop, RecvierNAK, BLKDrop, responseNAK, &receiver},   //收取文件内容但是校验失败，丢弃数据
        { BLKRecv, SenderLastBLK | RecvierACK, RecvEnd, reciveEnd, &receiver},   //收到包检测发现是最后一个包正确，那么结束断开
        { BLKRecv, SenderLastBLK | RecvierNAK, BLKRecv,  responseNAK,&receiver},   //收到包检测发现是最后一个包但不正确，那么继续处于BLKRecv状态
    };

    char ip[] = {"192.168.171.169"}; 
    initReceiver(&receiver, 3568, ip , sizeof(ip), recvStateShift, sizeof(recvStateShift) / sizeof(StateShift));

    shiftState(&receiver.stateShift, SenderConnect);
    printf("开始接收数据\n");
    unsigned long long totalSize = 0;
    float MBytes = totalSize / (1024.0 * 1024);
    float dispMBytes = 0;
    int fakeFailedPkt = 0;
    while(1){
        //从socket读取数据，以便更新事件
        Marker marker;
        int size = 0;
        if((size = readSocketToBuf(&receiver.netHandler)) > 0){
            LOG("本次读取到的字节数：%d", size);
            totalSize += size;
            MBytes = totalSize / (1024.0 * 1024);
            if(MBytes - dispMBytes >= 1){
                dispMBytes = MBytes;
                printf("\r\033[K总接收字节量%lluB, %.2fMB", totalSize, dispMBytes);
                fflush(stdout);
            }
            
            //根据buf填充marker，并获取事件，然后由事件驱动状态改变
            fillMarker(&receiver, &marker);
            //得到maker之后同时要返回相应结果给发送端
            int evtId = getEvtId(&marker);
                     
            if(evtId == RecvierACK){
                int chance = rand() % 100 + 1;
                if(chance < FakeNetCondition){
                    fakeFailedPkt++;
                    evtId = RecvierNAK;
                    LOG("假装出错次数更新：%d",fakeFailedPkt);
                }
            }
            LOG("当前事件：%d", evtId);
            //buf状态必须更新，以及buf的数据怎么处理需要在转换状态之前处理，或者在相应的回调函数里去处理buf里的内容
            //因为每次receiver的buf里只可能存放当前这一回合接收到的包
            //我选择在状态转换函数里处理接收到的data数据，并进行readPos更新
            shiftState(&receiver.stateShift, evtId);
        } 
        if(size == -2){
            break;  
        }
    }
}