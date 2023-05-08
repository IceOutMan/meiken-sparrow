#include "core.h"
#include <string.h>
#include <sys/stat.h>
#include "../include/utils.h"
#include "vm.h"
#include "../object/class.h"
#include "../object/obj_range.h"
#include "../compiler/compiler.h"
#include "core.script.inc"
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <time.h>

char* rootDir = NULL; //根目录

#define CORE_MODULE VT_TO_VALUE(VT_NULL)

// 返回值类型是Value类型，且是放在args[0]，args是 Value 数组
// RET_VALUE 的参数就是Value类型，无须转换直接赋值
// 它是后面 “RET_ 其他类型” 的基础
#define RET_VALUE(value)\
    do{\
        args[0] = value;\
        return true;\
    }while(0);

// 将obj转换为Value后作为返回值
#define RET_OBJ(objPtr) RET_VALUE(OBJ_TO_VALUE(objPtr))

// 将bool值转为Value后作为返回值
#define RET_BOOL(boolean) RET_VALUE(BOOL_TO_VALUE(boolean))
#define RET_NUM(num) RET_VALUE(NUM_TO_VALUE(num))
#define RET_NULL RET_VALUE(VT_TO_VALUE(VT_NULL))
#define RET_TRUE RET_VALUE(VT_TO_VALUE(VT_TRUE))
#define RET_FALSE RET_VALUE(VT_TO_VALUE(VT_FALSE))

// 设置线程报错 
#define SET_ERROR_FALSE(vmPtr, errMsg) \
    do{\
        vmPtr->curThread->errorObj = \
                OBJ_TO_VALUE(newObjString(vmPtr, errMsg, strlen(errMsg)));\
        return false;\
    }while(0);

// 绑定原生方法 func 到 classPtr 指向的类
#define PRIM_METHOD_BIND(classPtr, methodName, func) {\
    uint32_t length = strlen(methodName);\
    int globalIdx = getIndexFromSymbolTable(&vm->allMethodNames, methodName, length);\
    if(globalIdx == -1){\
        globalIdx = addSymbol(vm, &vm->allMethodNames, methodName, length);\
    }\
    Method method;\
    method.type = MT_PRIMITIVE;\
    method.primFn = func;\
    bindMethod(vm ,classPtr, (uint32_t)globalIdx, method);\
}

// 检验是否是函数
static bool validateFn(VM* vm, Value arg){
    if(VALUE_TO_OBJCLOSURE(arg)){
        return true;
    }
    vm->curThread->errorObj = OBJ_TO_VALUE(newObjString(vm, "argument must be a function!", 28));
    return false;
}

// 将数字转换为字符串
static ObjString* num2str(VM* vm, double num){
    // nan 不是一个确定的值，因此 nan 和 nan 是不能相等的
    if(num != num){
        return newObjString(vm, "nan", 3);
    }

    if( num == INFINITY){
        return newObjString(vm, "infinity", 8);
    }
    if(num == -INFINITY){
        return newObjString(vm, "-infinity", 9);
    }
    // 以下 24 字节的缓冲区足以容纳双京都到字符串的转换
    char buf[24] = {'\0'};
    int len = sprintf(buf, "%.14g", num);
    return newObjString(vm, buf, len);
}

// 判断 arg 是否为数字
static bool validateNum(VM* vm, Value arg){
    if(VALUE_IS_NUM(arg)){
        return true;
    }
    SET_ERROR_FALSE(vm, "argument must be number!");
}

// 判断 arg 是否为字符串
static bool validateString(VM* vm, Value arg){
    if(VALUE_IS_OBJSTR(arg)){
        return true;
    }
    SET_ERROR_FALSE(vm, "argument must be string!");
}


// ->  !object : object取反 ，结果为false
static bool primObjectNot(VM* vm UNUSED, Value* args){
    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

// args[0] == args[1] : 返回object是否相等
static bool primObjectEqual(VM* vm UNUSED, Value* args){
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[0], args[1]));
    RET_VALUE(boolValue);
}

//args[0] != args[1]: 返回object是否不等
static bool primObjectNotEqual(VM* vm UNUSED, Value* args) {
   Value boolValue = BOOL_TO_VALUE(!valueIsEqual(args[0], args[1]));
   RET_VALUE(boolValue);
}

// args[0] is args[1] : 类 args[0] 是否是 类 args[1] 的子类
// 此处路基存在疑问：遍历是否应该改为 thisClass = thisClass->superClass
static bool primObjectIs(VM* vm, Value* args){
    // args[1] 必须是 class
    if(!VALUE_IS_CLASS(args[1])){
        RUN_ERROR("argment must bu class!");
    }

    Class* thisClass = getClassOfObj(vm, args[0]);
    Class* baseClass = (Class*)(args[1].objHeader);

    // 有可能是多级继承，因此自上而下便利基类链
    while(baseClass != NULL){
        // 在某一级基类找到匹配，就设置返回值为VT_TRUE并返回
        if(thisClass == baseClass){
            RET_VALUE(VT_TO_VALUE(VT_TRUE));
        }
        baseClass = baseClass->superClass;
    }

    // 若未找到基类，说明不具备 is_a 关系
    RET_VALUE(VT_TO_VALUE(VT_FALSE));
}

// args[0].toString : 返回 args[0]所属class的名字
static bool primObjectToString(VM* vm UNUSED, Value* args){
    Class* class = args[0].objHeader->class;
    Value nameValue = OBJ_TO_VALUE(class->name);
    RET_VALUE(nameValue);
}

// args[0].type : 返回对象 args[0] 的类
static bool primObjectType(VM* vm, Value* args){
    Class* class = getClassOfObj(vm, args[0]);
    RET_OBJ(class);
}

// args[0].name ： 返回类名
static bool primClassName(VM* vm UNUSED, Value* args){
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

//  args[0].supertype: 返回args[0] 的基型
static bool primClassSupertype(VM* vm UNUSED, Value* args){
    Class* class = VALUE_TO_CLASS(args[0]);
    if(class->superClass != NULL){
        RET_OBJ(class->superClass);
    }
    RET_VALUE(VT_TO_VALUE(VT_NULL));
}

// args[0].toString :  返回类名
static bool primClassToString(VM* vm UNUSED, Value* args){
    RET_OBJ(VALUE_TO_CLASS(args[0])->name);
}

// args[0].same(args[1], args[2]) : 返回args[1] 和 args[2] 是否相等
static bool primObjectMetaSame(VM* vm UNUSED, Value* args){
    Value boolValue = BOOL_TO_VALUE(valueIsEqual(args[1], args[2]));
    RET_VALUE(boolValue);
}
// 返回 bool 的字符串形式： true 或 false
static bool primBoolToString(VM* vm, Value* args){
    ObjString* objString;
    if(VALUE_TO_BOOL(args[0])){  // 若为 VT_TRUE
        objString = newObjString(vm, "true", 4);
    }else{
        objString = newObjString(vm, "false", 5);
    }
    RET_OBJ(objString);
}

// bool 值取反
static bool primBoolNot(VM* vm UNUSED, Value* args){
    RET_BOOL(!VALUE_TO_BOOL(args[0]));
}

// 以下以大写字符开头的为类名，表示类（静态）方法调用
// Thread.new(func): 创建一个 thread 实例
static bool primThreadNew(VM* vm, Value* args){
    // 代码块为参数必为闭包
    if(!validateFn(vm, args[1])){
        return false;
    }
    ObjThread* objThread = newObjThread(vm, VALUE_TO_OBJCLOSURE(args[1]));

    // 使 stack[0] 为接收者，保持栈平衡
    objThread->stack[0] = VT_TO_VALUE(VT_NULL);
    objThread->esp++;
    RET_OBJ(objThread);
}

// Thread.abort(err): 以错误信息 err 为参数退出线程
static bool primThreadAbort(VM* vm, Value* args){
    // 此函数后续未处理，暂时放着
    vm->curThread->errorObj = args[1]; // 保存退出参数
    return VALUE_IS_NULL(args[1]);
}

// Thread.current: 返回当前的线程
static bool primThreadCurrent(VM* vm, Value* args UNUSED){
    RET_OBJ(vm->curThread);
}

// Thread.suspend(): 挂起当前线程 , 退出解析器
static bool primThreadSuspend(VM* vm, Value* args UNUSED){
    // 目前 suspend 操作只会退出虚拟机
    // 使 curThread 为 NULL ，虚拟机将退出
    vm->curThread = NULL;
    return false;
}

// Thread.yield(arg) 带参数让出CPU
static bool primThreadYieldWithArg(VM* vm, Value* args){
    ObjThread* curThread = vm->curThread;
    vm->curThread = curThread->caller;  // 使CPU控制权回到主调方

    curThread->caller = NULL; // 与调用者断开联系
    if(vm->curThread != NULL){
        // 如果当前线程有主调方，就将当前线程的返回值放到主调用方的栈顶
        vm->curThread->esp[-1] = args[1];

        // 对于 thread.yield(arg) 来说，回收 arg 的空间
        // 保留 thread 参数所在的空间，将来唤醒时用于存储yield 结果
        curThread->esp--;
    }

    return false;
}

// Thread.yield() 无参数让出CPU
static bool primThreadYieldWithoutArg(VM* vm, Value* args UNUSED){
    ObjThread* curThread = vm->curThread;
    vm->curThread = curThread->caller;  // 使CPU控制权回到主调方

    curThread->caller = NULL; // 与主调方断开联系

    if(vm->curThread != NULL){
        // 为了保持通用的栈结构，如果当前线程有主调方
        // 就将空值作为返回值放在主调方的栈顶
        vm->curThread->esp[-1] = VT_TO_VALUE(VT_NULL);
    }
    return false;
}

// 切换到下一个线程 nextThread
static bool switchThread(VM* vm, ObjThread* nextThread, Value* args, bool withArg){
    // 在下一个线程 nextThread 执行前，其主调线程应该为空
    if(nextThread->caller != NULL){
        RUN_ERROR("thread has been called!");
    }
    nextThread->caller = vm->curThread;

    if(nextThread->usedFrameNum == 0){
        // 只有已运行完毕的 thread 的 usedFrameNum 才是0
        SET_ERROR_FALSE(vm, "a finished thread can't' be switched to!");
    }

    // 如果call有参数，回收参数的空间
    // 只保留此栈顶用于存储 nextThread 返回后的结果
    if(withArg){
        vm->curThread->esp--;
    }

    ASSERT(nextThread->esp > nextThread->stack, "esp should be greater than stack!");
    // nextThread.call(arg) 中的 arg 作为 nextThread.yield 的返回值
    // 存储到 nextThread 的栈顶，否则押入NULL 保持栈平衡
    nextThread->esp[-1] = withArg ? args[1] : VT_TO_VALUE(VT_NULL);

    // 使当前线程指向 nextThread， 使之就绪
    vm->curThread = nextThread;

    // 返回false 以进入vm中的切换线程流程
    return false;
}

// objThread.call()
static bool primThreadCallWithoutArg(VM* vm, Value* args){
    return switchThread(vm, VALUE_TO_OBJTHREAD(args[0]), args, false);
}

// objThread.call(arg)
static bool primThreadCallWithArg(VM* vm, Value* args){
    return switchThread(vm, VALUE_TO_OBJTHREAD(args[0]), args, true);
}

// objThread.isDone 返回线程是否运完成
static bool primThreadIsDone(VM* vm UNUSED, Value* args) {
    // 获取 .isDone 的调用者
    ObjThread *objThread = VALUE_TO_OBJTHREAD(args[0]);
    RET_BOOL(objThread->usedFrameNum == 0 || !VALUE_IS_NULL(objThread->errorObj));
}

// Fn.new(_) ： 新建一个函数对象
static bool primFnNew(VM* vm, Value* args){
    // 代码块为参数 必为闭包
    if(!validateFn(vm, args[1])){
        return false;
    }

    // 直接返回函数闭包
    RET_VALUE(args[1]);
}

// 绑定 fn.call 的重载
static void bindFnOverloadCall(VM* vm , const char* sign){
    uint32_t index = ensureSymbolExist(vm, &vm->allMethodNames, sign, strlen(sign));
    // 构造 method
    Method method = {MT_FN_CALL, {0}};
    bindMethod(vm, vm->fnClass, index, method);
}

// null 取非
static bool primNullNot(VM* vm UNUSED, Value* args UNUSED){
    RET_VALUE(BOOL_TO_VALUE(true));
}

// null 的字符串化
static bool primNullToString(VM* vm, Value* args UNUSED){
    ObjString* objString = newObjString(vm ,"null", 4);
    RET_OBJ(objString);
}

// 将字符串转换为数字
static bool primNumFromString(VM* vm, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }
    ObjString* objString = VALUE_TO_OBJSTR(args[1]);

    // 空字符串返回 RETURN_NULL
    if(objString->value.length == 0){
        RET_NULL;
    }

    ASSERT(objString->value.start[objString->value.lenght] == '\0', "objString don't teminate!");
    errno = 0;
    char* endPtr;

    // 将字符串转换为double 型， 它会自动跳过前面的空白
    double num = strtod(objString->value.start, &endPtr);
    // 以 endPtr 是否等于 start + length 来判断不能转换的字符之后是否全是空白
    while(*endPtr != '\0' && isspace((unsigned char)*endPtr)){
        endPtr++;
    }

    if(errno == ERANGE){
        RUN_ERROR("string too large!");
    }

    // 如果字符串中不能转换的字符不全部是空白，字符串非法，返回NULL
    if(endPtr < objString->value.start + objString->value.length){
        RET_NULL;
    }

    // 至此，检查通过，返回正确结果
    RET_NUM(num);
}

// 返回圆周率
static bool primNumPi(VM* vm UNUSED, Value* args UNUSED){
    RET_NUM(3.14159265358979323846);
}

#define PRIM_NUM_INFIX(name, operator, type) \
    static bool name(VM* vm, Value* args){   \
            if(!validateNum(vm, args[1])){   \
                return false; \
            } \
            RET_##type(VALUE_TO_NUM(args[0]) operator VALUE_TO_NUM(args[1])); \
    }
PRIM_NUM_INFIX(primNumPlus, +, NUM);
PRIM_NUM_INFIX(primNumMinus, -, NUM);
PRIM_NUM_INFIX(primNumMul, *, NUM);
PRIM_NUM_INFIX(primNumDiv, /, NUM);
PRIM_NUM_INFIX(primNumGt, >, BOOL);
PRIM_NUM_INFIX(primNumGe, >=, BOOL);
PRIM_NUM_INFIX(primNumLt, <, BOOL);
PRIM_NUM_INFIX(primNumLe, <=, BOOL);
#undef PRIM_NUM_INFIX

#define PRIM_NUM_BIT(name, operator) \
    static bool name(VM* vm UNUSED, Value* args) { \
        if(!validateNum(vm, args[1])) { \
            return false;\
        }                            \
        uint32_t leftOperand = VALUE_TO_NUM(args[0]); \
        uint32_t rightOperand = VALUE_TO_NUM(args[1]);\
        RET_NUM(leftOperand operator rightOperand);\
    }
PRIM_NUM_BIT(primNumBitAnd, &);
PRIM_NUM_BIT(primNumBitOr, |);
PRIM_NUM_BIT(primNumBitShiftRight, >>);
PRIM_NUM_BIT(primNumBitShiftLeft, <<);
#undef PRIM_NUM_BIT

// 使用数学库
#define PRIM_NUM_MATH_FN(name, mathFn) \
    static bool name(VM* vm UNUSED, Value* args){ \
        RET_NUM(mathFn(VALUE_TO_NUM(args[0])));   \
    }
PRIM_NUM_MATH_FN(primNumAbs, fabs);
PRIM_NUM_MATH_FN(primNumAcos, acos);
PRIM_NUM_MATH_FN(primNumAsin, asin);
PRIM_NUM_MATH_FN(primNumAtan, atan);
PRIM_NUM_MATH_FN(primNumCeil, ceil);
PRIM_NUM_MATH_FN(primNumCos, cos);
PRIM_NUM_MATH_FN(primNumFloor, floor);
PRIM_NUM_MATH_FN(primNumNegate, -);
PRIM_NUM_MATH_FN(primNumSin, sin);
PRIM_NUM_MATH_FN(primNumSqrt, sqrt); // 开方
PRIM_NUM_MATH_FN(primNumTan, tan);
#undef PRIM_NUM_MATH_FN

// 这里用 fmod 实现浮点数取模
static bool primNumMod(VM* vm UNUSED, Value* args){
    if(!validateNum(vm, args[1])){
        return false;
    }
    RET_NUM(fmod(VALUE_TO_NUM(args[0]), VALUE_TO_NUM(args[1])));
}

// 数字取反
static bool primNumBitNot(VM* vm UNUSED, Value* args){
    RET_NUM(~(uint32_t)VALUE_TO_NUM(args[0]));
}
// [数字 from .. 数字 to]
static  bool primNumRange(VM* vm UNUSED, Value* args){
    if(!validateNum(vm, args[1])){
        return false;
    }

    double from = VALUE_TO_NUM(args[0]);
    double to  = VALUE_TO_NUM(args[1]);
    RET_OBJ(newObjRange(vm, from, to));
}

// atan2(args[1])
static bool primNumAtan2(VM* vm UNUSED, Value* args){
    if(!validateNum(vm, args[1])){
        return false;
    }

    RET_NUM(atan2(VALUE_TO_NUM(args[0]), VALUE_TO_NUM(args[1])));
}

// 返回小数部分
static bool primNumFraction(VM* vm UNUSED, Value* args){
    double dummyInteger;
    RET_NUM(modf(VALUE_TO_NUM(args[0]), &dummyInteger));
}

// 判断数字是否无穷大，不区分正负无穷大
static bool primNumIsInfinity(VM* vm UNUSED, Value* args){
    RET_BOOL(isinf(VALUE_TO_NUM(args[0])));
}

// 判断是否为数字
static bool primNumIsInteger(VM* vm UNUSED, Value* args){
    double num = VALUE_TO_NUM(args[0]);
    // 如果是 nan（不是一个数字）或无限大的数字就返回false
    if(isnan(num) || isinf(num)){
        RET_FALSE;
    }
    RET_BOOL(trunc(num) == num);
}

// 判断数字是否是nan
static bool primNumIsNan(VM* vm UNUSED, Value* args){
    RET_BOOL(isnan(VALUE_TO_NUM(args[0])));
}

// 数字转化为字符串
static bool primNumToString(VM* vm UNUSED, Value* args){
    RET_OBJ(num2str(vm, VALUE_TO_NUM(args[0])));
}

// 取数字的整数部分
static bool primNumTruncate(VM* vm UNUSED, Value* args){
    double integer;
    modf(VALUE_TO_NUM(args[0]), &integer);
    RET_NUM(integer);
}

// 判断两个数字是否相等
static bool primNumEqual(VM* vm UNUSED, Value* args){
    if(!validateNum(vm, args[1])){
        RET_FALSE;
    }
    RET_BOOL(VALUE_TO_NUM(args[0]) == VALUE_TO_NUM(args[1]));
}

// 判断两个数字是否不等
static bool primNumNotEqual(VM* vm UNUSED, Value* args){
    if(!validateNum(vm, args[1])){
       RET_TRUE;
    }
    RET_BOOL(VALUE_TO_NUM(args[0]) != VALUE_TO_NUM(args[1]));
}




// 返回核心模块 name 的 Value 结构
static Value getCoreClassValue(ObjModule* objModule, const char* name){
    int index = getIndexFromSymbolTable(&objModule->moduleVarName, name, strlen(name));
    if(index == -1){
        char id[MAX_ID_LEN] = {'\0'};
        memcpy(id, name, strlen(name));
        RUN_ERROR("something wring occur: missing core class \"%s\"!", id);
    }
    return objModule->moduleVarValue.datas[index];
}


// 读取源代码文件 const 在这里的意思是 path 在方法内不能被改变，否则编译报错
char* readFile(const char* path){
    FILE* file = fopen(path, "r");
    if(file == NULL){
        IO_ERROR("Could't open file \"%s\".", path);
    }

    struct stat fileStat;
    stat(path, &fileStat);
    size_t fileSize = fileStat.st_size;
    char* fileContent = (char*)malloc(fileSize+1);
    if(fileContent == NULL){
        MEM_ERROR("Could't allocate memory for reading file \"%s\".\n", path);
    }
    size_t numRead = fread(fileContent, sizeof(char), fileSize, file);
    if(numRead < fileSize){
        IO_ERROR("Could't read file \"%s\".\n", path);
    }
    fileContent[fileSize] = '\0';
    fclose(file);
    return fileContent; 
}

// table 中查找符合 symbol ， 找到后返回索引，否则返回-1
int getIndexFromSymbolTable(SymbolTable* table, const char* symbol, uint32_t length){
    ASSERT(length != 0, "length of symbol is 0!");
    uint32_t index = 0;
    while(index < table->count){
        if(length == table->datas[index].length &&
                memcmp(table->datas[index].str, symbol, length) == 0){
            return index;
        }
        index++;
    }
    return -1;
}

// 往table中添加符号symbol，返回其索引
int addSymbol(VM* vm, SymbolTable* table, const char* symbol, uint32_t length){
    ASSERT(length != 0, "length of symbol is 0!");
    String string;
    string.str = ALLOCATE_ARRAY(vm, char, length+1);
    memcpy(string.str, symbol, length);
    string.str[length] = '\0';
    string.length = length;
    StringBufferAdd(vm, table, string);
    return table->count-1;
}

// 从 modules 中获取名为 moduleName 的模块 
static ObjModule* getModule(VM* vm, Value moduleName){
    Value value = mapGet(vm->allModules, moduleName);
    if(value.type == VT_UNDEFINED){
        return NULL;
    }
    return VALUE_TO_OBJMODULE(value);
}

// 载入模块 moduleName 并编译
static ObjThread* loadModule(VM* vm, Value moduleName, const char* moduleCode){
    // 确保模块已载入到 vm->allModules
    // 先查看是否已导入了该模块，避免重新导入
    ObjModule* module = getModule(vm, moduleName);

    // 若该模块未加载先将其载入，并继承核心模块中的变量
    if(module == NULL){
        // 创建模块并添加到 vm->allModules
        ObjString* modName = VALUE_TO_OBJSTR(moduleName);
        ASSERT(modName->value.start[modName->value.length] == '\0', "string.value.start is not terminated!");

        module = newObjModule(vm, modName->value.start);
        mapSet(vm, vm->allModules, moduleName, OBJ_TO_VALUE(module));

        // 继承核心模块中的变量
        ObjModule* coreModule = getModule(vm, CORE_MODULE);
        uint32_t idx = 0;
        while(idx < coreModule->moduleVarName.count){
            defineModuleVar(vm, module, 
                    coreModule->moduleVarName.datas[idx].str,
                    strlen(coreModule->moduleVarName.datas[idx].str),
                    coreModule->moduleVarValue.datas[idx] );
            idx++;
        }
    }
    ObjFn* fn = compileModule(vm ,module, moduleCode);
    ObjClosure* objClosure = newObjClosure(vm, fn);
    ObjThread* moduleThread = newObjThread(vm, objClosure);

    return moduleThread;
}

// 定义类
static Class* defineClass(VM* vm, ObjModule* objModule, const char* name){
    // 1. 先创建类
    Class* class = newRawClass(vm, name, 0);
    // 2. 把类作为普通变量在模块中定义
    defineModuleVar(vm, objModule, name, strlen(name), OBJ_TO_VALUE(class));

    return class;
}

// 确保符号已经添加到符号表
int ensureSymbolExist(VM* vm, SymbolTable* table, const char* symbol, uint32_t length){

    int symbolIndex = getIndexFromSymbolTable(table, symbol, length);
    if(symbolIndex == -1){
        return addSymbol(vm, table, symbol, length);
    }
    return symbolIndex;
}


// 执行模块，目前为空， 桩函数
VMResult executeModule(VM* vm, Value moduleName, const char* moduleCode){
    ObjThread* objThread = loadModule(vm, moduleName, moduleCode);
    return executeInstruction(vm, objThread);
}

// 使 class->methods[index] = method
void bindMethod(VM* vm, Class* class, uint32_t index, Method method){
    if(index >= class->methods.count){
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

    // 继承基类方法
    uint32_t idx = 0;
    while(idx < superClass->methods.count){
        bindMethod(vm, subClass, idx, superClass->methods.datas[idx]);
        idx++;
    }
}


// 编译核心模块
void buildCore(VM* vm){
    // 和性模块不需要名字，模块也允许名字为空
    ObjModule* coreModule = newObjModule(vm, NULL); // NULL为和兴模块.name


    // 创建核心模块， 录入到 vm->allModules
    mapSet(vm, vm->allModules, CORE_MODULE, OBJ_TO_VALUE(coreModule));

    // 创建 object 类并绑定方法
    vm->objectClass = defineClass(vm, coreModule, "object");
    PRIM_METHOD_BIND(vm->objectClass, "!", primObjectNot);
    PRIM_METHOD_BIND(vm->objectClass, "==(_)", primObjectEqual);
    PRIM_METHOD_BIND(vm->objectClass, "!=(_)", primObjectNotEqual);
    PRIM_METHOD_BIND(vm->objectClass, "is(_)", primObjectIs);
    PRIM_METHOD_BIND(vm->objectClass, "toString", primObjectToString);
    PRIM_METHOD_BIND(vm->objectClass, "type", primObjectType);

    // 定义classOfClass类，是素有meta类的meta类和基类
    vm->classOfClass = defineClass(vm, coreModule, "class");

    // objectClass 是任务类的基类
    bindSuperClass(vm, vm->classOfClass, vm->objectClass);

    PRIM_METHOD_BIND(vm->classOfClass, "name", primClassName);
    PRIM_METHOD_BIND(vm->classOfClass, "supertype", primClassSupertype);
    PRIM_METHOD_BIND(vm->classOfClass, "toString", primClassToString);

    //定义object类的元信息类objectMetaclass,它无须挂载到vm
    Class* objectMetaclass = defineClass(vm, coreModule, "objectMeta");

    //classOfClass类是所有meta类的meta类和基类
    bindSuperClass(vm, objectMetaclass, vm->classOfClass);

    //类型比较
    PRIM_METHOD_BIND(objectMetaclass, "same(_,_)", primObjectMetaSame);

    //绑定各自的meta类
    vm->objectClass->objHeader.class = objectMetaclass;
    objectMetaclass->objHeader.class = vm->classOfClass;
    vm->classOfClass->objHeader.class = vm->classOfClass; //元信息类回路,meta类终点

    // 执行核心模块
    executeModule(vm, CORE_MODULE, coreModuleCode);

    // Bool类定义在 core.script.inc 中，将其挂在到 Bool 类 到 vm->boolClass
    vm->boolClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "Bool"));
    PRIM_METHOD_BIND(vm->boolClass, "toString", primBoolToString);
    PRIM_METHOD_BIND(vm->boolClass, "!", primBoolNot);

    // Thread 类也是 core.script.inc 中定义的
    // 将其挂载到 vm->threadClass 并补充原生方法
    vm->threadClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "Thread"));
    // 以下是类方法
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "new(_)", primThreadNew);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "abort(_)", primThreadAbort);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "current", primThreadCurrent);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "suspend()", primThreadSuspend);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "yield(_)", primThreadYieldWithArg);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "yield()", primThreadYieldWithoutArg);
    // 以下是实例方法
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "call()", primThreadCallWithoutArg);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "call(_)", primThreadCallWithArg);
    PRIM_METHOD_BIND(vm->threadClass->objHeader.class, "isDone", primThreadIsDone);

    // 绑定函数类
    vm->fnClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "Fn"));
    PRIM_METHOD_BIND(vm->fnClass->objHeader.class, "new(_)", primFnNew);

    // 绑定 call 的重载方法
    bindFnOverloadCall(vm, "call()");
    bindFnOverloadCall(vm, "call(_)");
    bindFnOverloadCall(vm, "call(_,_)");
    bindFnOverloadCall(vm, "call(_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)");
    bindFnOverloadCall(vm, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)");

    // 绑定NULL类的方法
    vm->nullClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "Null"));
    PRIM_METHOD_BIND(vm->nullClass, "!", primNullNot);
    PRIM_METHOD_BIND(vm->nullClass, "toString", primNullToString);

    // 绑定 num 类方法
    vm->numClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "Num"));
    // 类方法
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "fromString(_)", primNumFromString);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "pi", primNumPi);
    // 实例方法
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "+(_)", primNumPlus);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "-(_)", primNumMinus);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "*(_)", primNumMul);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "/(_)", primNumDiv);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, ">(_)", primNumGt);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, ">=(_)", primNumGe);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "<(_)", primNumLt);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "<=(_)", primNumLe);

    // 位运算
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "&(_)", primNumBitAnd);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "|(_)", primNumBitOr);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, ">>(_)", primNumBitShiftRight);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "<<(_)", primNumBitShiftLeft);

    // 以上都是通过 rules 中 INFIX_OPERATOR 解析
    // 少数符号依然是 INFIX_OPERATOR 解析
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "abs", primNumAbs);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "acos", primNumAcos);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "asin", primNumAsin);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "atan", primNumAtan);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "ceil", primNumCeil);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "cos", primNumCos);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "floor", primNumFloor);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "-", primNumNegate);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "sin", primNumSin);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "sqrt", primNumSqrt);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "tan", primNumTan);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "%(_)", primNumMod);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "~", primNumBitNot);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "..(_)", primNumRange);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "atan(_)", primNumAtan2);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "fraction", primNumFraction);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "isInfinity", primNumIsInfinity);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "isInteger", primNumIsInteger);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "isNan", primNumIsNan);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "toString", primNumToString);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "==(_)", primNumEqual);
    PRIM_METHOD_BIND(vm->numClass->objHeader.class, "!=(_)", primNumNotEqual);

}
