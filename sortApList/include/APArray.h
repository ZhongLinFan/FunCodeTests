#pragma once
#include <stdbool.h> //bool

#define MAX_NUMS 1000

struct AP {
	int channel;
	char* name;	//外部开辟内存
	char macAddr[18];
	char* protectedAccess;	//外部开辟内存
	int signalDB;
	char protocol[8];
	char field7[10];
	char field8[5];
	char field9[4];
};

//加载文件构建APArray
//注意这里是struct AP** 因为元素类型为struct AP*
//返回值为成功加载ap的个数，最大值为maxLen，最小值为0
int loadFile(char* path, struct AP** apArr, int maxLen);
//辅助填充函数
void fillField(struct AP* data, int fieldCount, char* lineData, int start, int end);
//辅助定位函数
int locateFieldStart(char* lineData, int fieldCount);
int locateFieldEnd(char* lineData, int fieldCount);	//返回的是结束的下一位
//打印AP*数组
void printAPArray(struct AP** apArr, int len);
//两个结构体的大小比较
bool comparisonAP(struct AP* left, struct AP* right);
//排序算法辅助函数 一次划分
int partitionAPList(struct AP** apArr, int low, int high);
//快排
void qSortAPList(struct AP** apArr, int low, int high);