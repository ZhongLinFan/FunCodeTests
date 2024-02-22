#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "fileFlow.h"

#define DataCtlNums 32  //测试了几个文件，发现128足够了，但是为了测试逻辑，改成32

static FileFlow sender;

bool initSender(const char* name, unsigned short port, const char* ip, int ipSize, 
     StateShift* sendStateShift, int stateShiftSize){
    initSenderFileHandler(&sender.fileHandler, name, 0);
    initShifter(&sender.stateShift, ConnectedWait, sendStateShift, stateShiftSize);
    initSenderNetHandler(&sender.netHandler, port, ip, ipSize);
}

bool connectReceiver(){
    int fd = sender.netHandler.cfd;
    struct sockaddr_in addr = sender.netHandler.addr;
    int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    int flag = fcntl(fd, F_SETFL, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK|flag);    //连接之后设置为非阻塞
    LOG("客户端的cfd为：%d", fd);
    if(ret == -1)
    {
        perror("connect");
        exit(0);
    }
}

void wait2Connected(void* data){
    printf("已连接至接收端\n");
}

//这个函数的data是传入传出参数
void sendData(void* data){
    if(data == NULL){
        printf("sendData 错误\n");
        exit(0);
    }
    int* size = (int*)data;
    NetHandler* netHandler = &sender.netHandler;

    int sendBytes = 0;
    while((sendBytes = writeBufToSocket(netHandler, *size, false)) == -1);
    if(sendBytes == -2){
        perror("sendBlock");
        exit(0);
    }
    LOG("写入字节数：%d, cfd为:%d", sendBytes, sender.netHandler.cfd);
    *(int*)data = sendBytes; //本次写入到对端的实际字节个数，这个和writeBufToSocket里lastSendBytes的更新好像重复了
}

void sendSyn(){
    char syn[] = {SYN, SYN};
    copyDataToNetBuf(&sender.netHandler, syn, 2);
    //读过的状态是否更新，refreshNetBuf()和这里的更新不一样，refreshNetBuf是接收到receiver的反馈之后是否更新buf，这里是读过buf之后是否更新，两者刚好是你来我往
    int size = 2; 
    sendData(&size);
}

bool checkSendData(char* data, int dataSize, int avaiableSize, int* ctlByte){
    int i = 0;
    int ctlByteCount = 0;
    int size = sizeof(uint16_t) * DataCtlNums;
    uint16_t* ctlPos = (uint16_t*)malloc(size);
    if(ctlPos == NULL){
        printf("空间开辟失败\n");
        exit(0);
    }
    for(i = 0; i < dataSize; i++){
        if(isCtlByte(data[i])){
            if(ctlByteCount >= (size / sizeof(uint16_t))){
                size *= 2;
                //LOG("正在realloc，size为%d\n", size);
                uint16_t* temp = realloc(ctlPos, size);
                if(temp == NULL){
                    printf("空间开辟失败\n");
                    free (ctlPos);
                    exit(0);
                }
                ctlPos = temp;
            }
            ctlPos[ctlByteCount++] = i;
        }
    }
    *ctlByte = ctlByteCount;
    LOG("本次发送一共有%d个控制字符", ctlByteCount);

    if(ctlByteCount <= avaiableSize){
        int j = ctlByteCount-1;
        int segStart = ctlPos[j];
        int segEnd = dataSize;  //注意这个不能减1，segEnd代表当前节结束的下一个字节的位置
        for(; j >= 0;){
            //上面那么复杂的原因是想在这倒着move，因为这样可以减少拷贝量
            LOG("第%d段移动信息,移动的起始地址:%d(%02x),目的地址:%d(%02x),移动字节数：%d, 插入的DLE位置:%d", 
                j, segStart, *(data + segStart), segStart + ctlByteCount, *(data + segStart + ctlByteCount), 
                segEnd - segStart, segStart + ctlByteCount-1);
            memmove(data + segStart + ctlByteCount, data + segStart, segEnd - segStart);
            *(data + segStart + ctlByteCount-1) = DLE;
            
            ctlByteCount--;
            segEnd = segStart;
            j--;
            segStart = ctlPos[j];
        }
        free(ctlPos);
        return true;
    }
    free(ctlPos);  //这个容易忘
    //avaiableSize不够用
    return false;
}

int sendBlock(int checkedStart, int bbcOffset){
    NetHandler* netHandler = &sender.netHandler;

    //1、在net的buf里填充DLE
    int dleNums = 0;
    int avaiableSize = MaxBufLen - netHandler->writePos;
    //注意如果是block，前面有可能有unread的情况，所以为了统一，这里使用writePos定位
    while(!checkSendData(netHandler->buf + checkedStart, netHandler->writePos - checkedStart, avaiableSize, &dleNums)){
        avaiableSize = adjustNetBuf(netHandler, dleNums+1);    //+1是为了BBC
    }
    //需要处理writePos，因为增加了字节数
    netHandler->writePos += dleNums;
    LOG("插入的dle数量：%d",dleNums);
    //2、然后计算BBC，必须要在填充之后计算BBC，因为BBC只能包含不包括插入的DLE在内的，但是又包括DLE不超过1024个字节的数据
    //所以如果不知道插入的DLE有多少，就不知道BBC需要计算到哪里
    //这里题目中定义了FTD只发一次，所以这里不校验是否大于1024了，也就是假定插入了DLE之后，依然不超过1024
    int sendSize = 0;
    uint16_t bbc = 0;
    //这里必须使用readPos，为了和普通的block统一，因为可能上次check完成，但是只发了其中的一部分，所以这里需要从readPos获取
    //不过因为FTD需要前面加入两个字符，所以这里需要+2
    char* bbcStart = netHandler->buf + netHandler->readPos + bbcOffset;
    char* bbcEnd = netHandler->buf + netHandler->writePos;
    //必须要检查最后两个通信字节是否是DLE+通信字符的组合，否则会有可能分两次发送，这样肯定错了，很坑。。。
    //所以getBCC的时候需要额外的检查一下最后一个字节是否是DLE，如果是，那么少发一个字节
    //不对，必须判断前两位，因为有可能最后两位都为DLE，结果分两次发送了，肯定不对。。。。。太坑
    //检查当前要发的最后一位为DLE是怕最后一位的下一位的通信字符为控制字符，而判断最后两位是怕倒数第一位为通信字符DLE
    //然后根据前面一位判断的逻辑，把前面刚好通信字符DLE会放到下一次，这样就会断开，但是这样还是会推到倒数第三位的判断上。。。
    //这需要考虑全局的DLE的数量分布，因为如果不知道，会无法判断最后一位是DLE的情况下是否应该发送
    //可以这样考虑，在getBCCSender的时候，统计截止字符前DLE的数量，备用，如果发送的字符不是DLE，那啥都用不上
    //如果是DLE，那么就需要遍历前面保存DLE的信息，如果两个DLE连在一起，当其不存在，如果DLE中间有断开的，那么当断开的前面不存在
    //直到判断到截止字符前，如果还剩一个DLE，那么可以截止字符和剩下的这个匹配，如果前面的DLE一个都不剩，那么这个DLE是和后面的一个匹配
    //上面判断的逻辑考虑了全是连在一起的DLE
    //不过完全可以倒着遍历，并且统计第一次不是DLE时DLE的个数，如果为奇数，那么可以认为是当前截止位置和前面的DLE是一起的，所以可以发当前位置
    //如果为偶数，那么当前的DLE和后面的是一起的，就不能发最后一位
    int sendBodyBytes = getBCCSender(bbcStart, bbcEnd-bbcStart, &bbc);
    LOG("bbc值%d，要发送的body字节数量：%d",bbc, sendBodyBytes);

    //3、将etb和bbc插入到本次要发送的最后一个字符后面 
    memmove(bbcStart+sendBodyBytes+2, bbcStart+sendBodyBytes, bbcEnd-(bbcStart+sendBodyBytes));
    *(bbcStart+sendBodyBytes) = sender.stateShift.state == LastBLK ? ETX : ETB;
    *(bbcStart+sendBodyBytes+1) = bbc;
    netHandler->writePos += 2;

    //4、本次需要发送的字节段
    char* sendEnd = bbcStart + sendBodyBytes + 2;
    char* sendStart = bbcStart - bbcOffset;
    int sendBytes = sendEnd - sendStart;  //本次发送的实际字节个数
    LOG("writeBufToSocket要发送的字节数量：%d", sendBytes);

#ifdef DEBUG
    LOG("要写的字节信息全貌:");
    int count = 0;
    for(int i = 0; i < netHandler->writePos - netHandler->readPos; i++){
        printf("%02x ", netHandler->buf[netHandler->readPos + i]);
        count++;
    }
    LOG("要写的字节数量：%d", count);
    LOG("已check但本次不发送的字节全貌:");
    for(; count < netHandler->writePos - checkedStart ; count++){ //netHandler->writePos长度
        printf("%02x ", netHandler->buf[netHandler->readPos + count]);
    }
#endif

    sendData(&sendBytes);

    return sendBytes; //本次写入到对端的实际字节个数
}

//执行这个之前，肯定执行过refreshNetBuf了，这个时候readPos和writePos肯定为0
void sendFTD(void* data){
    NetHandler* netHandler = &sender.netHandler;
    //为了和发送普通block的流程统一，应该先将要发送的数据拷贝到net的buf里
    char soh_stx[] = {SOH, STX};
    copyDataToNetBuf(netHandler, soh_stx, 2);
    char* ftd = (char*)&sender.fileHandler.fileInfo;
    int size = sizeof(FileInfo);
    copyDataToNetBuf(netHandler, ftd, size);
    LOG("当前的要发送的字节数：%d",netHandler->writePos -  netHandler->readPos);
    
    sendBlock(netHandler->readPos+2, 2);  //注意+2是为了过滤SOh和stx
    LOG("sendFTD");
}

//发送方每次read之前NetHandler里的buf里的数据可能还有用，因为如果收到NAK，那么需要重新发送之前的数据
//read并且，shiftState之后，也不能清空buf，直到收到ACK才可以更新上次读之后的状态，也就是更新readPos
//read最好阻塞读，一来一回
void refreshNetBuf(int evtId){
    //不管是哪种情况，都先清理本次读取到的ACK或者NAK数据
    int size = sizeof(uint16_t);
    sender.netHandler.writePos -= size;
    //如果evtId == RecvierACK，那么把上次发送的字节量更新，也就是readPos，这样unread的即为上次check完，但没发的
    //如果为NAK，那么说明上次发错了，就什么都不需要动了，因为上次发送的状态没有更新，上次发送的字节量也记录了
    if(evtId == RecvierACK || evtId == SenderLastBLK || evtId == RecvierNAK){
        if(evtId == RecvierACK || evtId == SenderLastBLK){
            sender.netHandler.readPos += sender.netHandler.lastSendBytes;
        }
        //sender->netHandler.readPos -= size; //这里不能更新readPos，fillMarker时不是正常读取的
        int readPos = sender.netHandler.readPos;
        int writePos = sender.netHandler.writePos;
        if(writePos < 0 || writePos < readPos){
            printf("refreshNetBuf内部更新错误, writePos:%d, readPos:%d\n", writePos, readPos);
            exit(0);
        }
    }else{
        printf("事件ID未知\n"); //可以不用管，因为shiftState也找不到，所以啥都不做，这个时候肯定卡死了，因为双方的接力突然中断了
        exit(0); //也可以退出
    }
}


//每次执行这个之前，肯定执行过refreshNetBuf了，代表是要发送新的数据了，这个时候readPos和writePos之间是上次没有发送完的数据，但是已经check的数据
void sendFileData(void* data){

    NetHandler* netHandler = &sender.netHandler;
    FileHandler* fileHandler =  &sender.fileHandler;
    //先记录上次的没有来得及发送出去的起止位置
    int checkedStart = netHandler->readPos;
    int checkedEnd = netHandler->writePos;
    int copyFileBytes = 1024 - (checkedEnd - checkedStart); //每次先读让net的buf里有1024个字节
    bool isAdjustNetBuf = false;
    bool isLastFileBlk = false;
    //拷贝指定数量数据到buf
    int realCopyBytes = copyFileBufToNetBuf(netHandler, fileHandler, copyFileBytes, &isAdjustNetBuf);
#ifdef DEBUG
    LOG("当前从file拷贝到net的字节数：%d", realCopyBytes);
    LOG("未checked之前net BUF全貌：");
    for(int i = netHandler->readPos; i < netHandler->writePos; i++){
        printf("%02x ", netHandler->buf[i]);
    }
#endif
    //copyFileBufToNetBuf之后，可能net进行移动了，所以readPos和writePos之间的checked段还需要更新
    //调整的话，肯定是前移，这个时候readPos肯定为0，所以可以以此为依据判断，或者在copyFileBufToNetBuf添加传出参数
    //还是用传出参数吧，更直观一些
    if(isAdjustNetBuf){
        checkedEnd -= checkedStart;
        checkedStart = 0;
    }

    //copyFileBytes < realCopyBytes如果小于说明已经读取到最后一块了，但是还没进入发送的最后一块，只有加上当前netbuf还没来得及发的小于1024才算进入
    //如果最后一块数据刚好copyFileBytes == realCopyBytes呢
    //条件应该是isEnd为true，并且file的readPos == writePos的时候，可以认定这次读取的是最后一块数据，这样就不用考虑上面的问题了
    //即使最后一块数据读出来发送不完，下次再执行sendFileData，下面这个条件还是为真，所以可以不用担心分多次发送的问题
    //但是如果最后一次读取到字节数，虽然小于像读取的，但是不会被置为isEnd。。。。所以上述判断条件不对，在这里直接判断feof(fileHandler->file)
    if(feof(fileHandler->file) && fileHandler->readPos == fileHandler->writePos){
        isLastFileBlk = true;
        LOG("是最后一块数据");
    }

    //肯定要判断而且必须要在sendBlock之前判断是否发送了最后一块，以便主动切换状态，如果在之后判断，那么说明最后一块已经发送出去了，填充的却是ETB
    //但是不填充DLE怎么知道最终发送的数据量大小，万一被拆分成两次发送呢，可以通过check判断一下当前最后一块数据的dle数量（而不填充）
    if(isLastFileBlk){
        int ctlByte = 0;
        int dataSize = netHandler->writePos - checkedStart;
        checkSendData(netHandler->buf + checkedStart, dataSize, 0, &ctlByte); //注意0很关键，可以保证只得到ctlByte
        //那么是否是最后一次的条件是什么？
        //最后一次发送的字节量小于1024，那如果最后一次刚好也发了1024个通信数据呢
        //也没关系，只要是isLastFileBlk，此时只要buf里的通信数据小于等于1024，那么就认为是发送最后一块的状态
        if(dataSize + ctlByte <= 1024){
            LOG("当前net的buf已经清空，已经进入到lastBlk状态");
            shiftState(&sender.stateShift, SenderLastBLK); //改变状态之后，填充的时候就可以直接判断该填充什么
        }
    }
   
    //（1）填充dle，填充bbc，发送数据
    //需要告诉这个函数需要填充dle的起始位置在哪，以及开始计算bbc的起始位置，因为发送FTD的时候前面可能还要两位控制字符
    //但是计算dle的时候会adjustNetBuf。。。所以不能给出绝对地址的而是告诉sendBlock在获得bbc的时候应该从readPos的第几个字节开始计算bbc
    //checkedStart则是相对于buf[0]的，所以两者有区别
    //（2）如果实际发送的字节数和已经bbc之后要发送的字节数对不上，那肯定bbc有问题，就会进入重复发送状态
    int sendBytes = sendBlock(checkedEnd, 0);  //注意不要传checkedStart，而是checkedEnd
    LOG("net的readPos:%d, sendBytes:%d, writePos:%d", netHandler->readPos, sendBytes, netHandler->writePos);
}

void destroySender(void* data){
    destroyFileFlow(&sender);
    printf("发送完毕！\n");
    exit(0);
}


int main(int argc, char* argv[]){
    if(argc < 2){
        printf("./sender filePath\n");
        return -1;
    }

    StateShift sendStateShift[] = {
        //可以把sender设置为静态变量，这样就不用传递sneder了。因为参数都一样
        { ConnectedWait, SenderConnect, SendConnected, wait2Connected, NULL },  //打印日志，主动事件
        { SendConnected, SenderSYN, SYN_BCC_Send, NULL, NULL },     //主动开启事件，驱动后续事件发生，所以为NULL
        { SYN_BCC_Send, RecvierACK, BLKOK, sendFTD, &sender },     //发送FTD（到这里，RecvierACK执行refreshNetBuf之后，buf的read和writePos肯定为0）
        { SYN_BCC_Send, RecvierNAK, SYN_BCC_Send, sendData, &sender.netHandler.lastSendBytes },
        { BLKOK, RecvierACK, BLKOK ,sendFileData, NULL},   //发送下一块数据
        { BLKOK, RecvierNAK, BLKERR , sendData, &sender.netHandler.lastSendBytes},  //发送上一次发的数据（当前buf缓冲区的内容）（可能发送FTD，也可能发送文件内容）
        { BLKERR, RecvierACK, BLKOK, sendFileData, NULL},  //发送下一块数据
        { BLKERR, RecvierNAK, BLKERR , sendData, &sender.netHandler.lastSendBytes}, //发送上一次发的数据（当前buf缓冲区的内容）
        { BLKOK, SenderLastBLK, LastBLK, NULL, NULL},      //sender需要主动驱动事件，因为是否为最后一块，只有sender读取发送时才知道
        { LastBLK, RecvierACK, SendEnd, destroySender, NULL},   //清理相关资源，退出程序
        { LastBLK, RecvierNAK, LastBLK , sendData, &sender.netHandler.lastSendBytes},   //发送上一次发送的数据
    };

    char ip[] = {"192.168.171.169"};
    initSender(argv[1], 3568, ip , sizeof(ip), sendStateShift, sizeof(sendStateShift) / sizeof(StateShift));
    connectReceiver(&sender);
    shiftState(&sender.stateShift, SenderConnect);  //主动驱动事件1
    sendSyn();
    shiftState(&sender.stateShift, SenderSYN);      //主动驱动事件2

    while(1){
        //从socket读取数据，以便更新事件
        Marker marker;
        int size = 0;
        if((size = readSocketToBuf(&sender.netHandler)) > 0){
            LOG("读取到的字节数：%d", size);
            //填充maker
            fillMarker(&sender, &marker);
            //得到maker之后，不需要返回结果给接收端，而是更新事件，驱动下一次写
            int evtId = getEvtId(&marker);
            LOG("当前事件：%d", evtId);
            refreshNetBuf(evtId);  //更新完之后可以确保此时net的buf里是上次发送的数据
            shiftState(&sender.stateShift, evtId);
            if(size == -2){
                break;  
            }
        }  
    }
}