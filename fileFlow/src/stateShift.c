#include "stateShift.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool initShifter(StateShifter* shifter, unsigned short initState, StateShift* shiftArr, int nums){
    if(shifter){
        shifter->state = initState;
        shifter->shiftNum = nums;
        for(int i = 0; i < nums; i++){
            StateShift* stateShift = (StateShift*)malloc(sizeof(StateShift));
            memcpy(stateShift, &shiftArr[i], sizeof(shiftArr[i]));
            shifter->shift[i] = stateShift;
            //LOG("i:%d, stateShift->curState:%d,  stateShift->evtId:%d", i , shiftArr[i].curState, shiftArr[i].evtId);
        }
    }
}

StateShift* findStateShift(StateShifter* shifter, unsigned short evtId){
    for(int i = 0; i < shifter->shiftNum; i++){
        if(shifter->state == shifter->shift[i]->curState && shifter->shift[i]->evtId == evtId){
            return shifter->shift[i];
        }
    }
    return NULL;
}

int shiftState(StateShifter* shifter, unsigned short evtId){
    StateShift* target = findStateShift(shifter, evtId);
    if(target == NULL){
        printf("状态转移出错，当前状态:%d, 事件id:%d\n", shifter->state, evtId);
        return -1;
    }
   
    shifter->state = target->nextState;
    LOG("状态转移为:%d", shifter->state);

    //必须转移完之后，在调用回调函数，因为可能存在递归转移状态，如果不提钱转移状态，那么最终还是会回到第一次的转移状态中
    if(target->handler){
        target->handler(target->data);
    }
    
}

void destroyShifter(StateShifter* shifter){
    if(shifter && shifter->shift){
         for(int i = 0; i < shifter->shiftNum; i++){
            StateShift* stateShift = shifter->shift[i];
            if(stateShift){
                free(stateShift);
            }
        }
    }
}
