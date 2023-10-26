#include "header_obj.h"
#include "class.h"

DEFINE_BUFFER_METHOD(Value);

// 初始化对象头
void initObjHeader(VM* vm, ObjHeader* objHeader, ObjType ObjType, Class* class){
    objHeader->type = ObjType;
    objHeader->isDark = false;
    objHeader->class = class;
    objHeader->next = vm->allObjects;
    vm->allObjects = objHeader; // 头插
}