#ifndef _VM_VM_H
#define _VM_VM_H
#include "../include/common.h"
#include "../object/header_obj.h"

struct vm {
    Class* stringClass; // 默认的 string对应的类
    Class* fnClass; // 指令流单元（函数，代码块，模块..)
    uint32_t allocatedBytes; // 累计已分配的内存量
    ObjHeader* allObjects;  // 所有已分配对象链表
    Parser* curParser;      // 当前词法分析器
};

void initVM(VM* vm);
VM* newVM(void);


#endif