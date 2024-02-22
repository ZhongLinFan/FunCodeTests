#ifndef _STATE_SHIFT_H_
#define _STATE_SHIFT_H_
#include "common.h"
#include <stdbool.h>

typedef struct{
    unsigned short curState;
    unsigned short evtId;
    unsigned short nextState;
    //handler
    void (*handler)(void*data);
    void* data;
}StateShift;

typedef struct{
    unsigned short state;
    unsigned short shiftNum;
    StateShift* shift[MaxShift];
}StateShifter;

bool initShifter(StateShifter* shifter, unsigned short initState, StateShift* shiftArr, int nums);
StateShift* findStateShift(StateShifter* shifter, unsigned short evtId);
int shiftState(StateShifter* shifter, unsigned short evtId);
void destroyShifter(StateShifter* shifter);

#endif  //_STATE_SHIFT_H_