#include <stdio.h>
#include <stdlib.h> //atoi
#include "APArray.h"
#include <string.h>
#include <time.h>//time()

char* protectedAcc[] = { "NONE", "WPAPSK/AES", "WPA2PSK/AES", "WPA1PSKWPA2PSK/AES", "WPA1PSKWPA2PSK/TKIPAES", "WPA2PSK/TKIPAES", "WEP" };
char* filed7[] = { "NONE", "ABOVE", "BELOW" };
char* filed9[] = {"NO", "YES"};

bool fillAPData(struct AP* ap) {
	int strLen = 0;
	ap->channel = rand() % 15;
	if (ap->name == NULL) {
		ap->name = (char*)malloc(33);	//因为位宽就为33（有一位需要存储\0，但是需要和后面有空格），更好的办法可以给各个位宽指定宏，这样就不用数字了
	}
	strLen = rand() % 33; //产生32位的长度，长度可以为0
	//针对每一位生成随机字符
	for (int i = 0; i < strLen; i++) {
		ap->name[i] = rand() % 95 + 32;//32-126的ascii都可以
	}
	ap->name[strLen] = '\0';

	strLen = 17; //mac地址就是17位
	sprintf(ap->macAddr, "%02x:%02x:%02x:%02x:%02x:%02x",	//需要指定0，否则会忽略高位的0
		rand() % 256, rand() % 256, rand() % 256,
		rand() % 256, rand() % 256, rand() % 256);	//sprintf也会追加/0

	if (ap->protectedAccess == NULL) {
		ap->protectedAccess = (char*)malloc(23);	//位宽就为23
	}
	strcpy(ap->protectedAccess, protectedAcc[rand() % 7]); //strcpy可以复制\0可以不用担心
	ap->signalDB = rand() % 101;
	strcpy(ap->protocol, "11b/g/n");	
	strcpy(ap->field7, filed7[rand() % 3]);
	strcpy(ap->field8, "In");
	strcpy(ap->field9, filed9[rand() % 2]);
	
	return true;
}

int main(int argc, char* argv[]) {
	srand((unsigned)time(NULL));	//随机数种子
	if (argc < 2) {
		printf("./a path apNums\n");
		return 0;
	}
	int apNums = atoi(argv[2]);
	if (apNums > MAX_NUMS || apNums <= 0) {
		printf("apNums过大或过小(1-%d)，已退出\n", MAX_NUMS);
		return -1;
	}
	//产生指定数目的ap个数，并写到resource文件夹中的APlist.txt中去
	char path[200] = { 0 };
	strcpy(path, argv[1]);
	FILE* fd = fopen(path, "w");	//覆盖写
	if (fd == NULL) {
		printf("打开文件失败\n");
		return -1;
	}

	struct AP ap;
	//初始化两个指针
	ap.name = NULL;
	ap.protectedAccess = NULL;
	while (apNums) {
		//填充ap
		if (fillAPData(&ap)) {
			//将ap以指定格式写入文件中
			char format1[] = { "%-4d%-33s%-20s%-23s%-9d%-8s%-7s%-3s%-9s\n" };
			char format2[] = { "%-4d%-33s%-20s%-23s%-9d%-8s%-7s%-4s%-8s\n" };
			char* targetFmt = strlen(ap.field9) == 2 ? format2 : format1;
			fprintf(fd, targetFmt, ap.channel, ap.name, ap.macAddr,
				ap.protectedAccess, ap.signalDB, ap.protocol,
				ap.field7, ap.field8, ap.field9);
			--apNums;
		}
	}
	//可以不free ap里面的两个new出来的,因为这里已经结束了
	fclose(fd);
	return 0;
}