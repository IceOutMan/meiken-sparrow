#ifndef _VM_VM_H
#define _VM_VM_H
#include "../include/common.h"
#include "../object/header_obj.h"
#include "../object/obj_map.h"
#include "../object/obj_thread.h"

// 为定义在opcode.inc 中的操作码加上前缀OPCODE_, 然后立马执行了undef，不让其他地方使用这个宏明
#define OPCODE_SLOTS(opcode, effect) OPCODE_##opcode,
typedef enum {
   #include "opcode.inc"
} OpCode;
// 由于使用了 undef，所以编译器报错了,这里没有问题，认为是一种警告
#undef OPCODE_SLOTS

typedef enum vmResult{
    VM_RESULT_SUCCESS,
    VM_RESULT_ERROR
} VMResult; // 虚拟机执行结果

struct vm {
    Class* classOfClass;
    Class* objectClass;
    Class* stringClass;
    Class* mapClass;
    Class* rangeClass;
    Class* listClass;
    Class* nullClass;
    Class* boolClass;
    Class* numClass;
    Class* fnClass; // 指令流单元（函数，代码块，模块..)
    Class* threadClass; 

    uint32_t allocatedBytes; // 累计已分配的内存量
    ObjHeader* allObjects;  // 所有已分配对象链表

    SymbolTable allMethodNames; // 所有类的方法名
    ObjMap* allModules;
    ObjThread* curThread; // 当前正在执行的线程

    Parser* curParser;      // 当前词法分析器
};

void initVM(VM* vm);
VM* newVM(void);


#endif