#include "obj_list.h"

// 新建 list 对象，元素个数为 elementNum
ObjList* newObjList(VM* vm, uint32_t elementNum){
    // 存储 list 元素的缓冲区
    Value* elementArray = NULL;

    // 先分配内存，后调用 initObjHeader , 避免 gc 无谓地遍历
    if(elementNum > 0){
        elementArray = ALLOCATE_ARRAY(vm ,Value, elementNum);
    }
    ObjList* objList = ALLOCATE(vm, ObjList);

    objList->elements.datas = elementArray;
    objList->elements.capacity = objList->elements.count = elementNum;
    initObjHeader(vm, &objList->objHeader, OT_LIST, vm->listClass);
    return objList;
}

// 在 objlist 中索引为 index 处插入 value，类似 list[index] = value
void insertElement(VM* vm, ObjList* objList, uint32_t index, Value value){
    if(index > objList->elements.count -1 ){
        RUN_ERROR("index out bounded!");
    }

    // 准备一个 Value 的空间以容纳新元素产生的空间波动
    // 既最后一个元素要后移1个空间
    ValueBufferAdd(vm, &objList->elements, VT_TO_VALUE(VT_NULL));

    // 下面使 index 后面的元素整体后移一位
    uint32_t idx = objList->elements.count  - 1;
    while(idx > index){
        objList->elements.datas[idx] = objList->elements.datas[idx-1];
        idx--;
    }

    // 在 index 处插入数值
    objList->elements.datas[index] = value;
}

// 调整 list 的容量
static void shrinkList(VM* vm, ObjList* objList, uint32_t newCapacity){
    uint32_t oldSize = objList->elements.capacity * sizeof(Value);
    uint32_t newSize = newCapacity * sizeof(Value);
    memManager(vm, objList->elements.datas, oldSize, newSize);
    objList->elements.capacity = newCapacity;
}

//  删除 list 中索引为 index 处 的元素，既删除 list[index]
Value removeElement(VM* vm, ObjList* objList, uint32_t index){
    Value valueRemoved = objList->elements.datas[index];

    // 使 index 后面的元素前移一位，覆盖 index 处的元素
    uint32_t idx = index;
    while(idx < objList->elements.count){
        objList->elements.datas[idx] = objList->elements.datas[idx+1];
        idx++;
    }

    // 若容量利用率过低就减少容量
    uint32_t  _capacity = objList->elements.capacity / CAPACITY_GROW_FACTOR;
    if(_capacity > objList->elements.count){
        shrinkList(vm, objList, _capacity);
    }

    objList->elements.count--;
    return valueRemoved;
}
























