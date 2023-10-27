#ifndef _OBJECT_THREAD_H
#define _OBJECT_THREAD_H

#include "obj_fn.h"

typedef struct objThread {
    ObjHeader objHeader;

    Value* stack; // 运行时栈的栈底
    Value* esp;   // 运行时栈的栈顶｜随着指令执行移动
    uint32_t stackCapacity; // 栈的容量

    Frame* frame;   // 调用框架
    uint32_t usedFrameNum; // 已使用的 frame 数量
    uint32_t frameCapacity; // frame 容量

    // 打开的 upvalue 的链表首节点
    ObjUpvalue* openUpvalues;
    // 当前 thread 的调用者
    struct objThread* caller;

    // 导致运行时错误的对象会放在此处，否则为空
    Value errorObj;

} ObjThread;


#endif