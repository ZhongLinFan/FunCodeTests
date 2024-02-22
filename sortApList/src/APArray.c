#include "APArray.h"
#include <stdio.h>
#include <stdlib.h> //atoi
#include <string.h>


int loadFile(char* path, struct AP** apArr, int maxLen) {
	//打开文件
	FILE* fd = fopen(path, "r");
	if (fd == NULL) {
		printf("打开文件失败\n");
		return 0;
	}
	//准备一个存放读取到一行数据的地方
	char tmpData[200] = { 0 }; //	确保能装下一行，否则会出错
	int lineCount = 0;
	//每次读取一行，读到文件尾或者数组长度行
	//fgets总是会在字符串后面加\0，所以不用担心
	while (fgets(tmpData, 200, fd) != NULL && lineCount < maxLen) {
		//将当前行，转为一个struct AP，并且放入到数组中，不需要字符串处理函数，因为复杂度未必比这好，这里只需要扫描一遍即可，字符串处理函数可能要反复的使用
		//当前处理的指针
		int fieldStart = 0;	//每个字段的开始指针
		int fieldEnd = 0;	//每个字段的结束指针
		int fieldCount = 0; //当前处理的第几个字段，因为填充字段时需要用到
		//当前行转换后的载体
		struct AP* data = (struct AP*)malloc(sizeof(struct AP));
		apArr[lineCount] = data;	//先将指针记下
		while (fieldCount != 9) {
			//字段起始截至定位思路，因为有些字段为空，所以必须手动指定每个字段的起始位置，否则肯定会定位错误，这是个必要条件，所以定位可以使用switch进行处理。
			//起始位置很好定位，只需要按照上面指定出即可
			//截至位置可以从下一个字段的起始位置往前找，找到第一个不是空格的，当然不能超过当前字段的起始位置
			//还可以使用sscanf解析，可能效果更好，更通用
			fieldStart = locateFieldStart(tmpData, fieldCount);
			fieldEnd = locateFieldEnd(tmpData, fieldCount);
			fillField(data, fieldCount, tmpData, fieldStart, fieldEnd);	//填充任务交给这个函数处理，注意传进去的j指向的是结束字段的下一个值
			fieldCount++;
		}
		//当前行处理完++
		lineCount++;
	}
	//关闭文件
	fclose(fd);
	return lineCount;
}

void fillField(struct AP* data, int fieldCount, char* lineData, int start, int end) {
	switch (fieldCount) {
	case 0:
	{
		char num[end - start + 1];
		strncpy(num, lineData + start, end - start);
		num[end - start + 1] = '\0';
		data->channel = atoi(num);
		break;
	}
	case 1:
	{
		data->name = (char*)malloc(end - start + 1);
		strncpy(data->name, lineData + start, end - start);
		data->name[end - start + 1] = '\0';
		break;
	}
	case 2:
	{
		strncpy(data->macAddr, lineData + start, end - start);
		data->macAddr[end - start + 1] = '\0';
		break;
	}
	case 3:
	{
		data->protectedAccess = (char*)malloc(end - start + 1);
		strncpy(data->protectedAccess, lineData + start, end - start);
		data->protectedAccess[end - start + 1] = '\0';
		break;
	}
	case 4:
	{
		char num[end - start + 1];
		strncpy(num, lineData + start, end - start);
		num[end - start + 1] = '\0';
		data->signalDB = atoi(num);
		break;
	}
	case 5:
	{
		strncpy(data->protocol, lineData + start, end - start);
		data->protocol[end - start + 1] = '\0';
		break;
	}
	case 6:
	{
		strncpy(data->field7, lineData + start, end - start);
		data->field7[end - start + 1] = '\0';
		break;
	}
	case 7:
	{
		strncpy(data->field8, lineData + start, end - start);
		data->field8[end - start + 1] = '\0';
		break;
	}
	case 8:
	{
		strncpy(data->field9, lineData + start, end - start);
		data->field9[end - start + 1] = '\0';
		break;
	}
	default:
		break;

	}
}

int locateFieldStart(char* lineData, int fieldCount) {
	switch (fieldCount) {
	case 0:return 0;
	case 1:return 4;
	case 2:return 37;
	case 3:return 57;
	case 4:return 80;
	case 5:return 89;
	case 6:return 97;
	case 7:return 104;
	case 8:return lineData[107] == ' ' ? 108 : 107;
	default:return 115;	//当前行的'\0'，只要不是上述字段都返回这个位置的指针，方便后续操作
	}
}

int locateFieldEnd(char* lineData, int fieldCount) {
	int nextStart = locateFieldStart(lineData, fieldCount + 1);
	int curStart = locateFieldStart(lineData, fieldCount);
	int j = nextStart - 1;
	while (lineData[j--] == ' ' && j >= curStart);	//出来之后j会指向当前字段有效位的前一个位置，所以下面需要+1
	j++;
	return j + 1; //不过这里返回下一位的位置更方便
}

void printAPArray(struct AP** apArr, int len) {
	char format1[] = { "%-4d%-33s%-20s%-23s%-9d%-8s%-7s%-3s%-9s\n" };
	char format2[] = { "%-4d%-33s%-20s%-23s%-9d%-8s%-7s%-4s%-8s\n" };
	for (int i = 0; i < len; i++) {
		char* targetFmt = strlen(apArr[i]->field9) == 2 ? format2 : format1;
		printf(targetFmt, apArr[i]->channel, apArr[i]->name, apArr[i]->macAddr,
			apArr[i]->protectedAccess, apArr[i]->signalDB, apArr[i]->protocol,
			apArr[i]->field7, apArr[i]->field8, apArr[i]->field9);
	}
}

bool comparisonAP(struct AP* left, struct AP* right) {
	if (left->signalDB != right->signalDB) {
		return left->signalDB >= right->signalDB;	//大于等于为true
	}
	//相等的话，比较channel
	return left->channel >= right->channel;	//大于等于为true
}

int partitionAPList(struct AP** apArr, int low, int high) {
	struct AP* pivot = apArr[low];	//先记住节点，方便后续交换
	while (low < high) {	//当两个指针碰到一起时就是pivot的位置
		//保证高位的位置全都大于等于中枢位置
		while (low < high && comparisonAP(apArr[high], pivot))--high;	//当高位指针大于等于中枢指针的时候，--high
		apArr[low] = apArr[high];	//当前已经不大于，那就交换到中枢位置左边
		//保证低的位置全都小于中枢位置
		while (low < high && !comparisonAP(apArr[low], pivot))++low;//当低位指针小于中枢指针的时候，++low
		apArr[high] = apArr[low];	//当前已经不小于，那就交换到中枢位置右边
		//一轮之后继续检查高位指针，也就是回到while
	}
	//找到了最终位置
	apArr[low] = pivot;
	return low;
}

void qSortAPList(struct AP** apArr, int low, int high) {
	//当low>=high的时候，说明当前序列不需要排序了
	if (low < high) {
		int pivotPos = partitionAPList(apArr, low, high);	//找到了当前序列的一个位置
		qSortAPList(apArr, low, pivotPos - 1);	//当前序列的左边排好了
		qSortAPList(apArr, pivotPos + 1, high);	//当前序列的右边排好了
	}
}