#include "header_obj.h"
#include "class.h"
#include "../vm/vm.h"


DEFINE_BUFFER_METHOD(Value)

// 初始化对象头
void initObjHeader(VM* vm, ObjHeader* objHeader, ObjType objType, Class* class){
    objHeader->type = objType;
    objHeader->isDark = false;
    objHeader->class = class;
    objHeader->next = vm->allObjects; // 头插法
    vm->allObjects = objHeader; // vm中的头指针指向最新的对象
}
