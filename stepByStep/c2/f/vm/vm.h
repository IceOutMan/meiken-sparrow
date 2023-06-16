#ifndef _VM_VM_H
#define _VM_VM_H

#include "../include/common.h"

struct vm{
    // 累计已分配的内存
    uint32_t allocatedBytes;
    // 当前词法分析器
    Parser* curParser; 
};

VM* newVM(void);
void initVM(VM* vm);

#endif

