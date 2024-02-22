#include <iostream>
#include <vector>
#include <string>
#include<utility>
#include<unordered_map>

//边节点
struct Arc {
	int head;
	int tail;
	//下面这两个是用不到的，因为链表不可能出现公共的节点的
	Arc* commonHeadNext;
	Arc* commonTailNext;
	Arc(int _head, int _tail) :head(_head), tail(_tail), commonHeadNext(nullptr), commonTailNext(nullptr) {};
};

//头节点
struct Vertex {
	std::string data;
	Arc* in;
	Arc* out;
	int inCount;
	int outCount;
	Vertex(const std::string& _data) :data(_data), in(nullptr), out(nullptr), inCount(0), outCount(0) {};
};

//链表的节点
struct LNode {
	std::string data;
	LNode* next;
	LNode(const std::string& _data) : data(_data), next(nullptr) {};
};

//思路：尝试建立十字链表，如果某个节点的入度或者出度大于等于2那么不可能是链表，
//如果入度为0的节点个数或者出度为0的节点个数大于1，那么也说明不是单链表
//如果建立成功，那么找到入度为0的节点，根据出度找到下一个节点，然后按照头插法得到其反转后的链表
//输出反转后的链表即可
//返回值：如果建立成功，返回入度为0的那个节点头节点（输入输出参数最终改为1个，因为后续发现创建链表的时候需要用到顶点表）
//如果建立失败，返回nullptr
Vertex* tryBuildGraph(std::vector<std::pair<std::string, std::string>>& input, std::vector<Vertex>& vexs) {
	//需要一个hash表记录顶点表数组的data和下标的位置关系，方便快速知道是否有当前节点
	std::unordered_map<std::string, int> indexMap;
	for (int i = 0; i < input.size(); i++) {
		std::string from = input[i].first;
		std::string to = input[i].second;
		//顶点表数组还不存在当前节点，那就加入到顶点表数组中
		if (indexMap.find(from) == indexMap.end()) {
			vexs.emplace_back(from);
			indexMap[from] = vexs.size() - 1;
		}
		
		if (indexMap.find(to) == indexMap.end()) {
			vexs.emplace_back(to);
			indexMap[to] = vexs.size() - 1;
		}

		//开始尝试建立当前边的关系
		//先检查from的合法性，如果出度已经为1，那么说明这次又指向了另一条边，不合法，返回false
		//如果为0，说明当前节点可以指向对应的to，但是to的合法与否现在还不确定

		//to的合法性，如果入度已经为1，那么说明这次又被其他节点指向了，不合法，返回false
		//如果为0，说明当前节点可以被from指向
		//也就是只有两个节点同时符合要求，那么才可以建立边的关系
		int fromIdx = indexMap[from];
		int toIdx = indexMap[to];
		Vertex* vexFrom = &vexs[fromIdx];
		Vertex* vexTo = &vexs[toIdx];
		if (vexFrom->outCount == 0 && vexTo->inCount == 0) {
			//建立边的关系
			vexFrom->outCount++;
			vexTo->inCount++;
			//out肯定为nullptr
			Arc* arc = new Arc(fromIdx, toIdx);
			vexFrom->out = arc;
			vexTo->in = arc;
			//std::cout << "建立：" << vexFrom->data << "->" << vexTo->data << std::endl;
		}
		else { 
			return nullptr;
		}
	}

	//检查入度为0的节点个数为0和出度为0的节点个数，只有两者都为1，才能确保是单链表，上面的for其实是确保了中间节点的入度和出度不超过1的合法性
	Vertex* head = nullptr;
	Vertex* tail = nullptr;
	//如果找到了当前节点那么对应标为true，如果都为true，又找到了度为0的节点，那么是errorFlag（实际上没用到，直接返回了。。。）
	bool headFlag = false, tailFlag = false, errorFlag = false;
	int inCount = 0, outCount = 0;	//临时变量，存储入度和出度的值
	for (int i = 0; i < vexs.size(); i++) {
		inCount = vexs[i].inCount;
		outCount = vexs[i].outCount;
		if (!headFlag && inCount == 0 && outCount == 1) {	//第二个条件是必要的，因为万一是孤立节点呢，那么inCount和outCount都为0
			head = &vexs[i];
			//std::cout << "head is true" << std::endl;
			headFlag = true;
			continue; //如果没有这个，那么还会对当前节点进行最后的那个if判断，显然不合适，而且会出错
		}
		if (!tailFlag && outCount == 0 && inCount == 1) {	//同上
			tail = &vexs[i];
			//std::cout << "tail is true" << std::endl;
			tailFlag = true;
			continue;	//同上
		}
		//如果都为true，然后又有度为0的节点，显然不对，可能存在中断，那就不是一个链表了，返回空即可
		if (headFlag && tailFlag && (outCount == 0 || inCount == 0))return nullptr;
	}
	//到这如果head不为空，那么就说明合法的单链表，如果为空，那就是不合法，返回的是nullptr
	return head;
}

//头插建立单链表
LNode* buildList(Vertex* graph, const std::vector<Vertex>& vexs) {
	//思路，访问当前节点，然后插入头，通过Arc* out的tail节点;访问下一个节点继续插入，直到out为nullptr
	const Vertex* gVertex = graph;
	LNode* lHeader = nullptr;
	LNode* lNode = nullptr;
	while (true) {
		if (lHeader == nullptr) {
			lHeader = new LNode(gVertex->data);
		}
		else {
			lNode = lHeader;
			LNode* node = new LNode(gVertex->data);
			node->next = lNode;
			lHeader = node;
		}
		//如果为nullptr，说明当前节点（当前节点已经在上面处理过了），已经到结尾了，断掉即可
		if (gVertex->out == nullptr) {
			break;
		}
		gVertex = &vexs[gVertex->out->tail];
	}
	//std::cout << "单链表建立完成" << std::endl;
	return lHeader;
}

//访问单链表
void visitList(LNode* list) {
	LNode* p = list;
	while (p != nullptr) {
		std::cout << p->data << std::endl;
		p = p->next;
	}
}

int main()
{
	std::vector<std::pair<std::string, std::string>> input;
	std::string from = "";
	std::string to = "";
	while (std::cin >> from >> to) {
		input.push_back(std::make_pair(from, to));
	}
	//顶点表数组
	std::vector<Vertex> vexs;
	Vertex* head = tryBuildGraph(input, vexs);

	if (head == nullptr) {
		std::cout << "NO";
	}
	else {
		//头插建立单链表
		LNode* list = buildList(head, vexs);
		//输出链表
		visitList(list);
	}
}

