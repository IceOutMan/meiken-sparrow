#ifndef _OBJECT_FN_H
#define _OBJECT_FN_H
#include "../include/utils.h"
#include "meta_obj.h"
typedef struct{
    char* fnName; // 函数名称
    IntBuffer lineNo; // 行号
} FnDebug; // 在函数中的调试结构

typedef struct{
    ObjHeader objHeader;
    ByteBuffer instrStream; // 函数编译后的指令流
    ValueBuffer constants; // 函数中的常量表

    ObjModule* module; // 本函数所属的模块

    // 本函数最多需要的栈空间，是栈使用空间的峰值
    uint32_t maxStackSlotUsedNum;
    uint32_t upvalueNum;    // 本函数所涵盖的 upvalue 数量
    uint8_t argNum;         // 函数期望的参数个数 
#if DEBUG
    FnDebug* debug;
#endif
} ObjFn; // 函数对象

typedef struct upvalue {
    ObjHeader objHeader;
    
    // 栈是个value类型的数组， localVarPtr 指向 upvalue 所关联的局部变量
    Value* localvarPtr;
    // 已被关闭的upvalue
    Value closedUpvalue;

    struct upvalue* next;   // 用以链接 openUpvalue链表
} ObjUpvalue; // upvalue 对象

typedef struct{
    ObjHeader objHeader;
    ObjFn* fn;  // 闭包中要饮用的函数

    ObjUpvalue* upvalues[0]; // 用于存储此函数的 closed upvalue
} ObjClosure; // 闭包对象

typedef struct {
    uint8_t* ip; // 程序计数器 指向下一个将被执行的指令

    // 在本frame中执行的闭包函数 
    ObjClosure* closure;

    // frame是共享 thread.stack 
    // 此项用于指向本frame所在thread运行时栈的起始地址
    Value* stackStart;
} Frame; // 调用框架 


#define INITIAL_FRAME_NUM 4 

ObjFn* nweObjFn(VM* vm, ObjModule* objModule, uint32_t slotNum);
ObjUpvalue* newObjUpvalue(VM* vm, Value* localVarPtr);
ObjClosure* newObjClosure(VM* vm, ObjFn* objFn);
ObjFn* newObjFn(VM* vm, ObjModule* objModule, uint32_t maxStackSlotUsedNum);

#endif