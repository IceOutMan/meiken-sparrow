#include "gc.h"
#include "../compiler/compiler.h"
#include "../object/obj_list.h"
#include "../object/obj_range.h"
#include "../parser/parser.h"
#include "../vm/vm.h"

#if DEBUG
#inlcude "debug.h"
#include <time.h>
#endif

// 标灰 obj： 既把 obj 收集到数组 vm->grays.grayObjects
void grayObject(VM *vm, ObjHeader *obj) {
    // 如果 isDark为 true 表示黑色，说明已经可达，直接返回
    if (obj == NULL || obj->isDark) return;

    // 标记为可达
    obj->isDark = true;

    // 若超过容量就扩容
    if (vm->grays.count >= vm->grays.capacity) {
        vm->grays.capacity = vm->grays.count * 2;
        vm->grays.grayObjects = (ObjHeader **) realloc(vm->grays.grayObjects, vm->grays.capacity * sizeof(ObjHeader *));
    }

    // 把 obj 添加到数组 grayObjects
    vm->grays.grayObjects[vm->grays.count++] = obj;
}

// 标灰 value
void grayValue(VM *vm, Value value) {
    // 只有对象才需要标记
    if (!VALUE_TO_OBJ(value)) {
        return;
    }

    grayObject(vm, VALUE_TO_OBJ(value));
}

// 标灰 buffer->datas 中的value
static void grayBuffer(VM *vm, ValueBuffer *buffer) {
    uint32_t idx = 0;
    while (idx < buffer->count) {
        grayValue(vm, buffer->datas[idx]);
        idx++;
    }
}

//

// 标黑 class
static void blackClass(VM *vm, Class *class) {
    // 标灰 meta 类
    grayObject(vm, (ObjHeader *) class->objHeader.class);

    // 标灰 父类
    grayObject(vm, (ObjHeader *) class->superClass);

    // 标灰方法
    uint32_t idx = 0;
    while (idx < class->methods.count) {
        if (class->methods.datas[idx].type == MT_SCRIPT) {
            grayObject(vm, (ObjHeader *) class->methods.datas[idx].obj);
        }
        idx++;
    }

    // 标灰类名
    grayObject(vm, (ObjHeader *) class->name);

    // 累计大小
    vm->allocatedBytes += sizeof(Class);
    vm->allocatedBytes += sizeof(Method) * class->methods.capacity;
}

static void blackClosure(VM *vm, ObjClosure *objClosure) {
    // 标灰比闭包中的函数
    grayObject(vm, (ObjHeader *) objClosure->fn);

    // 标灰 包中的 upvalue
    uint32_t idx = 0;
    while (idx < objClosure->fn->upvalueNum) {
        grayObject(vm, (ObjHeader *) objClosure->upvalues[idx]);
        idx++;
    }

    // 累计闭包的大小
    vm->allocatedBytes += sizeof(ObjClosure);
    vm->allocatedBytes += sizeof(ObjUpvalue *) * objClosure->fn->upvalueNum;
}

// 标黑 objTread
static void blackThread(VM *vm, ObjThread *objThread) {
    // 标灰frame
    uint32_t idx = 0;
    while (idx < objThread->usedFrameNum) {
        grayObject(vm, (ObjHeader *) objThread->frames[idx].closure);
        idx++;
    }

    // 标灰运行时栈中每个slot
    Value *slot = objThread->stack;
    while (slot < objThread->esp) {
        grayValue(vm, *slot);
        slot++;
    }

    // 标灰本线程中所有的 upvalue
    ObjUpvalue *upvalue = objThread->openUpvalues;
    while (upvalue != NULL) {
        grayObject(vm, (ObjHeader *) upvalue);
        upvalue = upvalue->next;
    }

    // 标灰 caller
    grayObject(vm, (ObjHeader *) objThread->caller);
    grayValue(vm, objThread->errorObj);

    // 累计线程大小
    vm->allocatedBytes += sizeof(ObjThread);
    vm->allocatedBytes += objThread->frameCapacity * sizeof(Frame);
    vm->allocatedBytes += objThread->stackCapacity * sizeof(Value);
}

// 标黑 fn
static void blackFn(VM *vm, ObjFn *fn) {
    // 标灰 常量
    grayBuffer(vm, &fn->constants);

    // 累计 ObjFn的空间
    vm->allocatedBytes += sizeof(ObjFn);
    vm->allocatedBytes += sizeof(uint8_t) * fn->instrStream.capacity;
    vm->allocatedBytes += sizeof(Value) * fn->constants.capacity;

#if DEBUG
    // 再加上 debug 信息占用的内存
    vm->allocatedBytes += sizeof(Int) * fn->instrStream.capacity;
#endif
}

// 标黑 objInstance
static void blackInstance(VM *vm, ObjInstance *objInstance) {
    // 标灰 元类
    grayObject(vm, (ObjHeader *) objInstance->objHeader.class);

    // 标灰实例中所有域， 域的个数在 class->fieldNum
    uint32_t idx = 0;
    while (idx < objInstance->objHeader.class->fieldNum) {
        grayValue(vm, objInstance->fields[idx]);
        idx++;
    }

    // 累计objInstance 空间
    vm->allocatedBytes += sizeof(ObjInstance);
    vm->allocatedBytes += sizeof(Value) * objInstance->objHeader.class->fieldNum;
}

// 标黑 objList
static void blackList(VM *vm, ObjList *objList) {
    // 标灰 list 的 elements
    grayBuffer(vm, &objList->elements);

    // 累计 objList 大小
    vm->allocatedBytes += sizeof(ObjList);
    vm->allocatedBytes += sizeof(Value) * objList->elements.capacity;
}

// 标黑 objMap
static void blackMap(VM *vm, ObjMap *objMap) {
    // 标灰所有 entry
    uint32_t idx = 0;
    while (idx < objMap->capacity) {
        Entry *entry = &objMap->entries[idx];
        // 跳过无效的 entry
        if (!VALUE_IS_UNDEFINED(entry->key)) {
            grayValue(vm, entry->key);
            grayValue(vm, entry->value);
        }
        idx++;
    }

    // 累计 ObjMap 大小
    vm->allocatedBytes += sizeof(ObjMap);
    vm->allocatedBytes += sizeof(Entry) * objMap->capacity;
}

// 标黑 objModule
static void blackModule(VM *vm, ObjModule *objModule) {
    // 标灰 模块中所有模块变量
    uint32_t idx = 0;
    while (idx < objModule->moduleVarValue.count) {
        grayValue(vm, objModule->moduleVarValue.datas[idx]);
        idx++;
    }

    // 标灰模块名
    grayObject(vm, (ObjHeader *) objModule->name);

    // 累计 ObjModule 大小
    vm->allocatedBytes += sizeof(ObjModule);
    vm->allocatedBytes += sizeof(String) * objModule->moduleVarName.capacity;
    vm->allocatedBytes += sizeof(Value) * objModule->moduleVarValue.capacity;
}

// 标黑 range
static void blackRange(VM *vm) {
    // ObjRange 中没有大数据，只有 from 和 to
    // 其空间属于 sizeof(ObjRange), 因此不用额外标记
    vm->allocatedBytes += sizeof(ObjRange);
}

// 标黑 objString
static void blackString(VM *vm, ObjString *objString) {
    // 累计ObjString 空间+1 是结尾的 '\0'
    vm->allocatedBytes += sizeof(ObjString) + objString->value.length + 1;
}

// 标黑 objUpvalue
static void blackUpvalue(VM *vm, ObjUpvalue *objUpvalue) {
    // 标灰 objUpvalue 的 closedUpvalue
    grayValue(vm, objUpvalue->closedUpvalue);

    // 累计 objUpvalue 大小
    vm->allocatedBytes += sizeof(ObjUpvalue);
}

// 标黑 obj
static void blackObject(VM *vm, ObjHeader *obj) {
#ifdef DEBUG
    printf("mark ");
    dupValue(OBJ_TO_VALUE(obj));
    printf(" @ %p\n", obj);
#endif
    // 根据对象类型分别标黑
    switch (obj->type) {
        case OT_CLASS:
            blackClass(vm, (Class *) obj);
            break;
        case OT_CLOSURE:
            blackClosure(vm, (ObjClosure *) obj);
            break;
        case OT_THREAD:
            blackThread(vm, (ObjThread *) obj);
            break;
        case OT_FUNCTION:
            blackFn(vm, (ObjFn *) obj);
            break;
        case OT_INSTANCE:
            blackInstance(vm, (ObjInstance *) obj);
            break;
        case OT_LIST:
            blackList(vm, (ObjList *) obj);
            break;
        case OT_MAP:
            blackMap(vm, (ObjMap *) obj);
            break;
        case OT_MODULE:
            blackModule(vm, (ObjModule *) obj);
            break;
        case OT_RANGE:
            blackRange(vm);
            break;
        case OT_STRING:
            blackString(vm, (ObjString *) obj);
            break;
        case OT_UPVALUE:
            blackUpvalue(vm, (ObjUpvalue *) obj);
            break;
    }
}

// 标黑那些已经标记灰的对象，既保留那些标灰的对象
static void blackObjectInGray(VM *vm) {
    // 所有要保留的对象都已经收集到了 vm->grays.grayObjects 中，现在逐一标黑
    while (vm->grays.count > 0) {
        ObjHeader *objHeader = vm->grays.grayObjects[--vm->grays.count];
        blackObject(vm, objHeader);
    }
}

// 释放 obj 自身及占用的内存
void freeObject(VM *vm, ObjHeader *obj) {
#ifdef DEBUG
    printf("free ");
    dumpValue(OBJ_TO_VALUE(obj));
    printf(" @ %p\n", obj);
#endif
    // 根据对象类型分别处理
    switch (obj->type) {
        case OT_CLASS:
            MethodBufferClear(vm, &((Class *) obj)->methods);
            break;
        case OT_THREAD: {
            ObjThread *objThread = (ObjThread *) obj;
            DEALLOCATE(vm, objThread->frames);
            DEALLOCATE(vm, objThread->stack);
            break;
        }

        case OT_FUNCTION: {
            ObjFn *fn = (ObjFn *) obj;
            ValueBufferClear(vm, &fn->constants);
            ByteBufferClear(vm, &fn->instrStream);
        #if DEBUG
            IntBufferClear(vm, &fn->debug->lineNo);
            DEALLOCATE(vm, fn->debug->fnName);
            DEALLOCATE(vm, fn->debug);
        #endif
            break;
        }

        case OT_LIST:
            ValueBufferClear(vm, &((ObjList*)obj)->elements);
            break;
        case OT_MAP:
            DEALLOCATE(vm, ((ObjMap*)obj)->entries);
            break;

        case OT_MODULE:
            StringBufferClear(vm, &((ObjModule*)obj)->moduleVarName);
            ValueBufferClear(vm, &((ObjModule*)obj)->moduleVarValue);
            break;

        case OT_STRING:
        case OT_RANGE:
        case OT_CLOSURE:
        case OT_INSTANCE:
        case OT_UPVALUE:
            break;
    }
    // 最后释放自己
    DEALLOCATE(vm, obj);
}

// 立即运行垃圾回收器去释放未使用的内存
void startGC(VM* vm){
#ifdef DEBUG
    double startTime = (double)clock() / CLOCKS_PER_SEC;
    uint32_t before = vm->allocatedBytes;
    printf("-- gc before:%d nextGC:%d vm:%p --\n", before, vm->config.nextGG, vm);
#endif
// 标记阶段： 标记需要保留的对象
    // 将 allocatedBytes 置0，便于精确统计回收后的总分配内存大小
    vm->allocatedBytes = 0;

    // allModules 不能被释放
    grayObject(vm, (ObjHeader*)vm->allModules);

    // 标灰 tmpRoots 数组中的对象（不可达但是不想被回收，白名单）
    uint32_t idx = 0;
    while(idx < vm->tmpRootNum){
        grayObject(vm, vm->tmpRoots[idx]);
        idx++;
    }

    // 标灰当前线程，不能被回收
    grayObject(vm, (ObjHeader*)vm->curThread);

    // 编译过程中若申的内存过高就标灰编译单元
    if(vm->curParser != NULL){
       ASSERT(vm->curParser->curCompileUnit != NULL, "grayCompileUnit only be called while compiling!");
       grayCompileUnit(vm, vm->curParser->curCompileUnit);

    }
    // 置黑所有灰色对象（保留的对象）
    blackObjectInGray(vm);

// 清扫阶段： 回收白色对象(垃圾对象)
    ObjHeader** obj = &vm->allObjects;
    while(*obj != NULL){
        // 回收白色对象
        if(!((*obj)->isDark)){
            ObjHeader* unreached = *obj;
            *obj = unreached->next;
            freeObject(vm, unreached);
        }else{
            // 如果已经是黑色对象，为了下一次gc重新判定，现将其恢复未标记状态，避免永远不被回收
            (*obj)->isDark = false;
            obj = &(*obj)->next;
        }
    }

    // 更新下一次触发 gc 的阈值
    vm->config.nextGC = vm->allocatedBytes * vm->config.heapGrowthFactor;
    if(vm->config.nextGC < vm->config.minHeapSize){
        vm->config.nextGC = vm->config.minHeapSize;
    }
#ifdef DEBUG
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - startTime;
    printf("GC %lu before, %lu after (%lu collected), next at %lu. take %.3fs.\n",
            (unsigned long)before,
            (unsigned long)vm->allocatedBytes,
            (unsigned long)(before - vm->allocatedBytes),
            (unsigned long)vm->config.nextGC,
            elapsed);
#endif
}





