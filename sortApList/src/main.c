#include "APArray.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("./a filePath\n");
		return -1;
	}
	printf("需要处理的文件：%s\n", argv[1]);
	//存指针，后续遍历开销更小
	//MAX_NUMS为支持的最大读取行数
	struct AP* apArr[MAX_NUMS];
	//加载文件到数组
	int len = loadFile(argv[1], apArr, MAX_NUMS); //数组实际长度

	printf("加载个数为：%d\n", len);
	if (len == 0 || len == MAX_NUMS) {
		//加载个数为0或最大值需要提醒一下
		printf("加载异常\n");
	}
	//打印，测试是否正确加载
	printf("排序前：\n");
	printAPArray(apArr, len);
	//排序
	qSortAPList(apArr, 0, len - 1);
	//打印结果到屏幕
	printf("\n排序后：\n");
	printAPArray(apArr, len);
	//销毁数组，可做可不做，因为已经return 0
	return 0;
}