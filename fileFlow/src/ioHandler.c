#include "ioHandler.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h> //不是#include <error.h>
#include <netinet/in.h> //TCP_NODELAY
#include <netinet/tcp.h> //TCP_NODELAY
#include <fcntl.h>
#include <unistd.h>

bool initFileHandler(FileHandler* fileHandler){
    if(fileHandler){
        fileHandler->readPos == 0;  //当前格未读
        fileHandler->writePos = 0;  //当前格未写
        fileHandler->isEnd = false;
        fileHandler->file = NULL;
        fileHandler->bufLen = 0;
        memset(fileHandler->buf, 0, MaxBufLen);
        return true;
    }
    return false;
}

bool initSenderFileHandler(FileHandler* fileHandler, const char* name, uint8_t version){
    if(fileHandler){
        initFileHandler(fileHandler);
        struct stat fileState;
        int res = stat(name, &fileState);
        if(res == -1){
            perror("initFileHandler");
            return false;
        }
        fileHandler->fileInfo.fileSize = fileState.st_size;
        fileHandler->fileInfo.lastModifyTime = fileState.st_mtime;

        int nameSize = strlen(name);
        fileHandler->fileInfo.fileNameLen = nameSize;
        memcpy(fileHandler->fileInfo.name, name, nameSize);
        fileHandler->fileInfo.name[nameSize] = '\0';
        fileHandler->fileInfo.extraFieldLen = 0;
        fileHandler->fileInfo.extraField[0] = '\0';
        fileHandler->fileInfo.version = version;

        fileHandler->file = fopen(name, "rb");
        if(fileHandler->file == NULL){
            perror("initFileHandler");
            return false;
        }
        return true;
    }
    return false;
}

bool initRecvierFileHandler(FileHandler* fileHandler){
    initFileHandler(fileHandler);
    return true;
}

int readFileToBuf(FileHandler* fileHandler){
    //每次都会读满buf
    int count = 0;
    //size为0也可以，直接move，为什么每次读取之间要move，减少read系统调用
    //所以上层调用该函数，应该先判断当前buf的数据够不够用，如果不够用，再调用这个函数
    int size = fileHandler->writePos - fileHandler->readPos;
    if(size >= 0){
        memmove(fileHandler->buf, fileHandler->buf + fileHandler->readPos, size);
        fileHandler->writePos = size;
        fileHandler->readPos = 0;
    }else{
        printf("readFileToBuf error\n");
        exit(0);
    }
      
    int avaiableSize = MaxBufLen - fileHandler->writePos;
    if(!feof(fileHandler->file)){
        count = fread(fileHandler->buf + fileHandler->writePos, sizeof(char), avaiableSize, fileHandler->file); //writePos别忘记
        fileHandler->writePos += count;
       
#ifdef DEBUG
        LOG("读取文件字节数:%d", count);
        LOG("当前file BUF全貌：");
        for(int i = fileHandler->readPos; i < fileHandler->writePos; i++){
            printf("%02x ", fileHandler->buf[i]);
        }
#endif
    }else{
        fileHandler->isEnd = true;
        printf("已读取完毕\n");
    }
    return count;
}

int writeBufToFile(FileHandler* fileHandler){

}

void destroyFileHandler(FileHandler* fileHandler){
    if(fileHandler &&  fileHandler->file){
        fclose (fileHandler->file);
    }
}

//网络io
bool initNetHandler(NetHandler* netHandler, unsigned short port, const char* ip, int ipSize){
    struct sockaddr_in* addr = &netHandler->addr;
    addr->sin_family = AF_INET;
    netHandler->readPos = 0;
    netHandler->writePos = 0;
    netHandler->lastSendBytes = 0;
    netHandler->lfd = 0;
    netHandler->cfd = 0;
    addr->sin_port = htons(port);  
    inet_pton(AF_INET, ip, &addr->sin_addr.s_addr);
    netHandler->port = port;
    memcpy(netHandler->ip, ip, ipSize);
}

bool initSenderNetHandler(NetHandler* netHandler, unsigned short port, const char* ip,  int ipSize){
    initNetHandler(netHandler, port, ip, ipSize);
    netHandler->cfd = socket(AF_INET, SOCK_STREAM, 0);
    if(netHandler->cfd == -1)
    {
        perror("socket");
        exit(0);
    }

}

bool initRecvierNetHandler(NetHandler* netHandler, unsigned short port, const char* ip,  int ipSize){
    initNetHandler(netHandler, port, ip, ipSize);
    netHandler->lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(netHandler->lfd == -1)
    {
        perror("socket");
        exit(0);
    }

    int opt = 1;
    setsockopt(netHandler->lfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));
   
    int ret = bind(netHandler->lfd, (struct sockaddr*)&netHandler->addr, sizeof(netHandler->addr));
    if(ret == -1)
    {
        perror("bind");
        exit(0);
    }

    ret = listen(netHandler->lfd, 128);
    if(ret == -1)
    {
        perror("listen");
        exit(0);
    }

    struct sockaddr_in cliaddr;
    int clilen = sizeof(cliaddr);
    netHandler->cfd = accept(netHandler->lfd, (struct sockaddr*)&cliaddr, &clilen);
    if(netHandler->cfd == -1)
    {
        perror("accept");
        exit(0);
    }
    int flag = fcntl(netHandler->cfd, F_SETFL, 0);
    fcntl(netHandler->cfd, F_SETFL, O_NONBLOCK|flag);
    //设置成TCP_NODELAY，否则小字节读不出来，这样的话1个字节也会触发
    //int op = 1;
    //setsockopt(netHandler->cfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(int));
    printf("服务器已和客户端建立连接：创建的cfd为：%d, 客户端信息如下,port:%d, ip:%s\n", 
        netHandler->cfd, ntohs(cliaddr.sin_port), inet_ntoa(cliaddr.sin_addr));
}


int readSocketToBuf(NetHandler* netHandler){
    int singleLen = 0;
    int totalLen = 0;
    //每次读应该都是读到buf的起始位置的，因为读完，之后肯定要处理，处理之后这些数据就没用了
    int avaiableSize = adjustNetBuf(netHandler, 1030);   //尝试调整大小
    //LOG("当前读取到buf的起始位置：%d， avaiableSize:%d", netHandler->writePos, avaiableSize);
    while ((singleLen = read(netHandler->cfd, netHandler->buf + netHandler->writePos, avaiableSize)) > 0) {
		netHandler->writePos += singleLen;
        totalLen += singleLen;
	}
	if (singleLen == -1 && errno == EINTR) {
		printf("singleLen == -1 && errno == EINTR\n");
        return -1;
	}
	else if (singleLen == -1 && errno == EAGAIN) {
		//LOG("读数据完成，buf的结束位置：%d", netHandler->writePos);
		return totalLen;
	}else if (singleLen == 0) {     //不能用writePos，因为上面的有可能不设置writePos
		printf("对方断开了连接\n");
        //后续工作 TODO，或者在上层后续工作
		exit(0);
	}
	else {
		perror("read");
        return -2;
	}
}

int writeBufToSocket(NetHandler* netHandler, int size, bool isFreshBuf){
    LOG("正在往socket的cfd：%d里写数据", netHandler->cfd);
    int ret = 0;
    int unreadSize = netHandler->writePos - netHandler->readPos;
    size = unreadSize < size ? unreadSize : size;
    do {
		ret = write(netHandler->cfd, netHandler->buf + netHandler->readPos, size);  
	} while (ret == -1 && errno == EINTR);  

	if (ret == -1) {
        if(errno == EAGAIN){
            return -1;
        }
        perror("writeBufToSocket");
		return -2;
	}
    netHandler->lastSendBytes = ret;
    
#ifdef DEBUG
    LOG("发送到对端之后，当前net BUF还剩余字节数:%d", netHandler->writePos - ret - netHandler->readPos);
    if(netHandler->readPos + ret != netHandler->writePos){
        for(int i = netHandler->readPos + ret; i < netHandler->writePos; i++){
            printf("%02x ", netHandler->buf[i]);
        }
    }
#endif

    if(isFreshBuf){
        netHandler->readPos += ret;  //可以直接加
    }
    LOG("writeBufToSocket：readPos%d, writePos:%d", netHandler->readPos, netHandler->writePos);
	return ret;
}

int adjustNetBuf(NetHandler* netHandler, int targetSize){
    int avaiableNetSize = MaxBufLen - netHandler->writePos;
    if(avaiableNetSize < targetSize){
        //将未读的数据拷贝到最前面，已处理的数据丢掉
        int moveSize = netHandler->writePos - netHandler->readPos;
        memmove(netHandler->buf, netHandler->buf + netHandler->readPos, moveSize);
        netHandler->readPos = 0;
        netHandler->writePos = moveSize;
    }
    avaiableNetSize = MaxBufLen - netHandler->writePos;
    return avaiableNetSize;
}

int copyDataToNetBuf(NetHandler* netHandler,  const char* data, int size){
    LOG("1copyDataToNetBuf：readPos%d, writePos:%d", netHandler->readPos, netHandler->writePos);
    if(size > 0){
        int avaiableNetSize = adjustNetBuf(netHandler, size);
        size = avaiableNetSize < size ? avaiableNetSize : size;
        memcpy(netHandler->buf+ netHandler->writePos, data, size);
        netHandler->writePos += size;  //可以直接加
    }
    LOG("2copyDataToNetBuf：readPos%d, writePos:%d", netHandler->readPos, netHandler->writePos);
    return size;
}


int copyFileBufToNetBuf(NetHandler* netHandler, FileHandler* fileHandler, int copySize, bool* isAdjustNetBuf){
    //每次尽可能的拷贝copySize个字节到net的buf里
    int size = 0;
    if(!fileHandler->isEnd){
        LOG("开始从文件buf拷贝数据");
        int unreadFileSize = fileHandler->writePos - fileHandler->readPos;
        if(unreadFileSize < copySize){    //如果file的buf不够，那么先填满file的buf
            readFileToBuf(fileHandler);
        }
        //将file的数据拷贝到net的buf
        unreadFileSize = fileHandler->writePos - fileHandler->readPos;
        size = unreadFileSize < copySize ? unreadFileSize : copySize;   //一般会是copySize
        LOG("file的unreadFileSize：%d, copySize:%d",unreadFileSize, copySize);
        //这个时候就要看net的buf够不够装了
        int avaiableNetSize = MaxBufLen - netHandler->writePos;
        if(avaiableNetSize < size){
            LOG("可用字节数%d不足", avaiableNetSize);
            avaiableNetSize = adjustNetBuf(netHandler, size);
            *isAdjustNetBuf = true;
            LOG("已将未读区域往前移动,writePos:%d", netHandler->writePos);
        }

        size = avaiableNetSize < size ? avaiableNetSize : size;   //一般会是之前size的值，要么就是上层copy值传的太大，导致即使腾空net的buf，还是不够
        LOG("当前要拷贝的字节大小:%d", size);
        memcpy(netHandler->buf + netHandler->writePos, fileHandler->buf + fileHandler->readPos, size);
#ifdef DEBUG
        LOG("从fileHandler拷贝到net的字节信息：");
        for(int i = netHandler->writePos; i < size; i++){
            printf("%02x ", netHandler->buf[i]);
        }
#endif
        fileHandler->readPos += size;   //可以直接加，因为size <= avaiableFileSize和avaiableNetSize
        netHandler->writePos += size; 
    }  
    return size;   //实际拷贝的字节数
}

void destroyNetHandler(NetHandler* netHandler){
    if(netHandler){
        close(netHandler->cfd);
        if(netHandler->lfd != 0){
            close(netHandler->lfd);
        }
    }
}