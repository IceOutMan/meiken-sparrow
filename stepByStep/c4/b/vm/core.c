#include "core.h"
#include <string.h>
#include <sys/stat.h>
#include "vm.h"
#include "../object/class.h"
#include "../compiler/compiler.h"
#include "../cli/cli.h"

char *rootDir = NULL;   //根目录

#define CORE_MODULE VT_TO_VALUE(VT_NULL)

// 返回类型是 Value 类型，且是放在 args[0] ，args 是 Value 数组
// RET_VALUE 的参数记忆是 Value 类型，无需转换直接赋值
// 它是后面 "RET_其他类型" 的基础
#define RET_VALUE(value) \
    do{ \
        args[0] = value; \
        return true; \
    }while(0);

// 将 obj 转换为 value 后作为返回值
#define RET_OBJ(objPtr) RET_VALUE(OBJ_TO_VALUE(objPtr))
// 将bool值转换为 Value 后作为返回值
#define RET_BOOL(boolean) RET_VALUE(BOOL_TO_VALUE(boolean))
#define RET_NUM(num) RET_VALUE(NUM_TO_VALUE(num))
#define RET_NULL RET_VALUE(VT_TO_VALUE(VT_NULL))
#define RET_TRUE RET_VALUE(VT_TO_VALUE(VT_TRUE))
#define RET_FALSE RET_VALUE(VT_TO_VALUE(VT_FALSE))

// 设置线程报错
#define SET_ERROR_FALSE(vmPtr, errMsg) \
    do { \
        vmPtr->curThread->errorObj = OBJ_TO_VALUE(newObjString(vmPtr, errMsg, strlen(errMsg))); \
        return false; \
    }while(0);

// 绑定方法 func 到 classPtr 指向的类
#define PRIM_METHOD_BIND(classPtr, methodName, func){ \
    uint32_t length = strlen(methodName);\
    int globalIdx = getIndexFromSymbolTable(&vm->allMethodNames, methodName, length);\
    if(globalIdx == -1){\
        globalIdx = addSymbol(vm, &vm->allMethodNames, methodName, length);\
    }\
    Method method;\
    method.type = MT_PRIMITIVE;\
    method.primFn = func;\
    bindMethod(vm, classPtr, (uint32_t)globalIdx, method);\
}

// 执行模块，目前为空，桩函数
VMResult executeModule(VM *vm, Value moduleName, const char *moduleCode) {
    return VM_RESULT_ERROR;
}



//读取源代码文件
char *readFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        IO_ERROR("Could`t open file \"%s\".\n", path);
    }

    struct stat fileStat;
    stat(path, &fileStat);
    size_t fileSize = fileStat.st_size;
    char *fileContent = (char *) malloc(fileSize + 1);
    if (fileContent == NULL) {
        MEM_ERROR("Could`t allocate memory for reading file \"%s\".\n", path);
    }

    size_t numRead = fread(fileContent, sizeof(char), fileSize, file);
    if (numRead < fileSize) {
        IO_ERROR("Could`t read file \"%s\".\n", path);
    }
    fileContent[fileSize] = '\0';

    fclose(file);
    return fileContent;
}

// !object: object 取反，结果为 false
static bool primObjectNot(VM *vm UNUSED, Value *args) {
    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

// args[0] == args[1] ： 返回 object 是否相等
static bool primObjectEqual(VM *vm UNUSED, Value *args) {
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[0], args[1]));
    RET_VALUE(boolValue);
}

// args[0] != args[1] : 返回 object 是否相等
static bool primObjectNotEqual(VM *vm UNUSED, Value *args) {
    Value boolValue = BOOL_TO_VALUE(!valueIsEqual(args[0], args[1]));
    RET_VALUE(boolValue);
}

// args[0] is args[1]: 类 args[0] 是否为类 args[1] 的子类
static bool primObjectIs(VM *vm UNUSED, Value *args) {
    // args[1] 必须是 class
    if (!VALUE_IS_CLASS(args[1])) {
        RUN_ERROR("argument must be class!");
    }

    Class *thisClass = getClassOfObj(vm, args[0]);
    Class *baseClass = (Class *) (args[1].objHeader);
    // 有可能是多级继承，因此自上而下遍历类链
    while (baseClass != NULL) {
        // 在某一级基类找到匹配，就设置返回值为 VT_TRUE 并返回
        if (thisClass == baseClass) {
            RET_VALUE(VT_TO_VALUE(VT_TRUE));
        }
        baseClass = baseClass->superClass;
    }

    // 若未找到基类，说明不具备 is_a 关系
    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

// args[0].toString: 返回 args[0]所属 class 的名字
static bool primObjectToString(VM *vm UNUSED, Value *args) {
    Class *class = args[0].objHeader->class;
    Value nameValue = OBJ_TO_VALUE(class->name);
    RET_VALUE(nameValue);
}

// args[0].type: 返回对象 args[0] 的类
static bool primObjectType(VM *vm, Value *args) {
    Class *class = getClassOfObj(vm, args[0]);
    RET_OBJ(class);
}

// args[0].name: 返回类名
static bool primClassName(VM *vm UNUSED, Value *args) {
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

// args[0].supertype: 返回 args[0] 的基类
static bool primClassSupertype(VM *vm UNUSED, Value *args) {
    Class *class = VALUE_TO_CLASS(args[0]);
    if (class->superClass != NULL) {
        RET_OBJ(class->superClass);
    }
    RET_VALUE(VT_TO_VALUE(VT_NULL));
}

// args[0].toString: 返回类名
static bool primClassToString(VM *vm UNUSED, Value *args) {
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

// args[0].same(args[1], args[2]) : 返回 args[1] 和 args[2] 是否相等
static bool primObjectmetaSame(VM *vm UNUSED, Value *args) {
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[1], args[2]));
    RET_VALUE(boolValue);
}

// table 中查找符号symbol，找到后返回索引，否则返回 -1
int getIndexFromSymbolTable(SymbolTable *table, const char *symbol, uint32_t length) {
    ASSERT(length != 0, "length of symbol is 0!");
    uint32_t index = 0;
    while (index < table->count) {
        if (length == table->datas[index].length && memcmp(table->datas[index].str, symbol, length) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

// 往 table 中添加符号 symbol，返回其索引
int addSymbol(VM *vm, SymbolTable *table, const char *symbol, uint32_t length) {
    ASSERT(length != 0, "length of symbol is 0!");
    String string;
    string.str = ALLOCATE_ARRAY(vm, char, length + 1);
    memcmp(string.str, symbol, length);
    string.str[length] = '\0';
    string.length = length;
    StringBufferAdd(vm, table, string);
    return table->count - 1;
}

// 定义类
static Class *defineClass(VM *vm, ObjModule *objModule, const char *name) {
    // 1. 创建类
    Class *class = newRawClass(vm, name, 0);

    // 2. 把类必做普通变量在模块中定义
    defineModuleVar(vm, objModule, name, strlen(name), OBJ_TO_VALUE(class));

    return class;

}

// 使 class->method[index] = method
void bindMethod(VM *vm, Class *class, uint32_t index, Method method) {
    if (index >= class->methods.count) {
        Method emptyPad = {MT_NONE, {0}};
        MethodBufferFillWrite(vm, &class->methods, emptyPad, index - class->methods.count + 1);
    }
    class->methods.datas[index] = method;
}

// 绑定基类
void bindSuperClass(VM* vm, Class* subClass, Class* superClass){
    subClass->superClass = superClass;

    // 继承基类属性数
    subClass->fieldNum += superClass->fieldNum;

    // 继承基类方法，这里算是直接拿了父类的方法的定义的对象了
    uint32_t idx = 0;
    while(idx < superClass->methods.count){
        bindMethod(vm, subClass, idx, superClass->methods.datas[idx]);
        idx++;
    }
}

// 编译核心模块
void buildCore(VM *vm) {
    // 核心模块不需要名字，模块也允许名字为空
    ObjModule *coreModule = newObjModule(vm, NULL); // NULL 为核心模块的 name
    // 创建核心模块， 录入到 vm->allModules
    mapSet(vm, vm->allModules, CORE_MODULE, OBJ_TO_VALUE(coreModule));

    // 创建 object 类并绑定方法
    vm->objectClass = defineClass(vm, coreModule, "object");
    PRIM_METHOD_BIND(vm->objectClass,"!", primObjectNot);
    PRIM_METHOD_BIND(vm->objectClass,"==(_)", primObjectEqual);
    PRIM_METHOD_BIND(vm->objectClass,"!=(_)", primObjectNotEqual);
    PRIM_METHOD_BIND(vm->objectClass,"is(_)", primObjectIs);
    PRIM_METHOD_BIND(vm->objectClass,"toString", primObjectToString);
    PRIM_METHOD_BIND(vm->objectClass,"type", primObjectType);

    // 定义 classOfClass 类，他是所有 meta 类和 meta 类的基类
    vm->classOfClass = defineClass(vm, coreModule,"class");
    // objectClass 是任何类的基类
    bindSuperClass(vm, vm->classOfClass, vm->objectClass);
    PRIM_METHOD_BIND(vm->classOfClass, "name", primClassName);
    PRIM_METHOD_BIND(vm->classOfClass, "supertype", primClassSupertype);
    PRIM_METHOD_BIND(vm->classOfClass, "toString", primClassToString);

    // 定义 object 类的元信息类 objectMetaClass，它无需挂载到 vm
    Class* objectMeatclass = defineClass(vm, coreModule, "objectMeta");

    // classOfClass 类是所有 meta 类的 meta 类和基类
    bindSuperClass(vm, objectMeatclass, vm->classOfClass);

    // 类型比较
    PRIM_METHOD_BIND(objectMeatclass, "same(_,_)", primObjectmetaSame);

    // 绑定各自的 meta 类
    vm->objectClass->objHeader.class = objectMeatclass;
    objectMeatclass->objHeader.class = vm->classOfClass;
    vm->classOfClass->objHeader.class = vm->classOfClass; // 元类信息回路，meta 类终点
}

