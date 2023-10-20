#ifndef _VM_VM_H
#define _VM_VM_H

#include "../include/common.h"

struct vm{
    Class* stringClass;
    Class* fnClass;
    uint32_t allocatedBytes;    // 累计已分配的内存
    Parser* curParser;  // 当前词法分析器
};

VM* newVM(void);
void initVM(VM* vm);

#endif

