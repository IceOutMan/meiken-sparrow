#include "vm.h"
#include "core.h"
#include <stdlib.h>
#include "../compiler/compiler.h"

// 初始化虚拟机
void initVM(VM* vm){
    vm->allocatedBytes = 0;
    vm->allObjects = NULL;
    vm->curParser = NULL;
    StringBufferInit(&vm->allMethodNames);
    vm->allModules = newObjMap(vm);
}

VM* newVM(){
    VM* vm = (VM*)malloc(sizeof(VM));
    if(vm == NULL){
        MEM_ERROR("allocate VM failed!");
    }
    initVM(vm);
    buildCore(vm);
    return vm;
}

// 确保 stack 有效
void ensureStack(VM* vm, ObjThread* objThread, uint32_t neededSlots){
    if(objThread->stackCapacity >= neededSlots){
        return;
    }

    uint32_t newStackCapacity = ceilToPowerOf2(neededSlots);
    ASSERT(newStackCapacity > objThread->stackCapacity, "newStackCapacity error!");

    // 记录原栈低以下面判断扩容后的栈是否是原地扩容
    Value* oldStackBottom = objThread->stack;

    uint32_t slotSize = sizeof(Value);
    objThread->stack = (Value*)memManager(vm, objThread->stack, objThread->stackCapacity * slotSize, newStackCapacity * slotSize);
    objThread->stackCapacity = newStackCapacity;

    // 为判断是否原地扩容
    long offset = objThread->stack - oldStackBottom;

    // 说明 os 无法在原地满足内容需求，重新分配了起始地址，下面要调整
    if(offset != 0 ){
        // 调整各堆栈框架的地址
        uint32_t idx = 0;
        while(idx < objThread->usedFrameNum){
            objThread->frames[idx++].stackStart += offset;
        }

        // 调整 open upValue
        ObjUpvalue* upvalue = objThread->openUpvalues;
        while(upvalue != NULL){
            upvalue->localVarPtr += offset;
            upvalue = upvalue->next;
        }

        // 更新栈顶
        objThread->esp += offset;
    }
}

// 为 objClosure在 objThread 中创建运行时栈
inline static void createFrame(VM* vm, ObjThread* objThread, ObjClosure* objClosure, int argNum){
    if(objThread->usedFrameNum + 1 > objThread->frameCapacity){ // 扩容
        uint32_t newCapacity = objThread->frameCapacity * 2;
        uint32_t frameSize = sizeof(Frame);
        objThread->frames = (Frame*)memManager(vm, objThread->frames,frameSize * objThread->frameCapacity, frameSize * newCapacity);
        objThread->frameCapacity = newCapacity;
    }

    // 栈大小等于栈顶-栈底
    uint32_t stackSlots = (uint32_t)(objThread->esp - objThread->stack);
    // 总共需要的栈的大小
    uint32_t neededSlots = stackSlots + objClosure->fn->maxStackSlotUsedNum;

    ensureStack(vm, objThread, neededSlots);

    // 准备上CPU
    prepareFrame(objThread, objClosure, objThread->esp - argNum);
}

// 关闭在栈中 slot 为lastSlot 及之上的 upvalue
static void closeUpvalue(ObjThread* objThread, Value* lastSlot){
    ObjUpvalue* upvalue = objThread->openUpvalues;
    while(upvalue != NULL && upvalue->localVarPtr >= lastSlot){
        // localVarPtr 改指向本结构中的closeUpvalue
        upvalue->closedUpvalue = *(upvalue->localVarPtr);
        upvalue->localVarPtr = &(upvalue->closedUpvalue);
        upvalue = upvalue->next;
    }
    objThread->openUpvalues = upvalue;
}
// 创建线程已打开的 upvalue 链表,并将 localVarPtr 所属的 upvalue 以降序插入到该链表
static ObjUpvalue* createOpenUpvalue(VM* vm, ObjThread* objThread, Value* localVarPtr){
    // 如果 openUpvalues 链表为空就创建
    if(objThread->openUpvalues == NULL){
        objThread->openUpvalues = newObjUpvalue(vm, localVarPtr);
        return objThread->openUpvalues;
    }

    // 下面以 upvalue.localVarPtr 降序组织 openUpvalues
    ObjUpvalue* preUpvalue = NULL;
    ObjUpvalue* upvalue = objThread->openUpvalues;

    // 后面的代码保证了 openUpvalues 按照降序组织
    // 下面向堆栈的底部遍历，直到找到合适的插入位置
    while(upvalue != NULL && upvalue->localVarPtr > localVarPtr){
        preUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    // 如果之前已插入了该upvalue则返回
    if(upvalue != NULL && upvalue->localVarPtr == localVarPtr){
        return upvalue;
    }

    // openUpvalues 中未找到 upvalue
    // 现在就创建新 upvalue，按照降序插入到链表
    ObjUpvalue* newUpvalue = newObjUpvalue(vm, localVarPtr);

    // 保证了 openUpvalues 首节点 upvalue->localVarPtr 的值是最高的
    if(preUpvalue == NULL){
        // 说上面 while 的循环未执行，新节点 (形参localVarPtr）的值大于链表首节点
        // 因此使链表节点指向它所在的新upvalue节点
        objThread->openUpvalues = newUpvalue;
    } else{
        // preUpvalue 已处于正确的位置
        preUpvalue->next = newUpvalue;
    }
    newUpvalue->next = upvalue;

    return newUpvalue; // 返回该节点
}

// 校验基类合法性
static void validateSuperClass(VM* vm, Value classNameValue, uint32_t fieldNum, Value superClassValue){
    // 首先确保 superClass 的类型是class
    if(!VALUE_IS_CLASS(superClassValue)){
        ObjString* classNameString = VALUE_TO_OBJSTR(classNameValue);
        RUN_ERROR("class \"%s\" 's superClass is not a valid class!", classNameString->value.start);
    }
    Class* superClass = VALUE_TO_CLASS(superClassValue);
    // 基类不允许为内建类
    if(superClass == vm->stringClass
                || superClass == vm->mapClass
                || superClass == vm->rangeClass
                || superClass == vm->listClass
                || superClass == vm->nullClass
                || superClass == vm->boolClass
                || superClass == vm->numClass
                || superClass == vm->fnClass
                || superClass == vm->threadClass)  {
        RUN_ERROR("superClass mustn't be a buildin class!");
    }

    // 子类也要继承基类的域
    // 故子类自己的域 + 基类域的数量不可以超过MAX_FIELD_NUM
    if(superClass->fieldNum + fieldNum > MAX_FIELD_NUM){
        RUN_ERROR("number of field including super exceed %d!", MAX_FIELD_NUM);
    }
}