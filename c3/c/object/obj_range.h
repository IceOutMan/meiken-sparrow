#ifndef _OBJECT_RANGE_H
#define _OBJECT_RANGE_H
#include "class.h"
typedef struct {
    ObjHeader objHeader;
    int from; // 范围的开始
    int to; // 范围的结束
} ObjRange; // range 对象

ObjRange* newObjRange(VM* vm, int from, int to);

#endif