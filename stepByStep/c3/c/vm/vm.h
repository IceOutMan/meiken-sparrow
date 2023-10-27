#ifndef _VM_VM_H
#define _VM_VM_H
#include "../include/common.h"
#include "../object/header_obj.h"
#include "../object/obj_map.h"

typedef enum vmResult{
    VM_RESULT_SUCCESS,
    VM_RESULT_ERROR
} VMResult;     // 虚拟机执行结果
// 如果执行无误，可以将字符码输出到文件缓存，避免下次重新编译

struct vm{
    Class* classOfClass;
    Class* objectClass;
    Class* stringClass;
    Class* mapClass;
    Class* rangeClass;
    Class* listClass;
    Class* nullClass;
    Class* boolClass;
    Class* numClass;
    Class* fnClass;
    Class* threadClass;
    uint32_t allocatedBytes;    // 累计已分配的内存
    ObjHeader* allObjects;      // 所有已分配对象链表
    SymbolTable allMethodNames;     // (所有）类的方法名
    ObjMap* allModules;
    ObjThread* curThread; // 当前正在执行的线程

    Parser* curParser;  // 当前词法分析器
};

VM* newVM(void);
void initVM(VM* vm);

#endif



