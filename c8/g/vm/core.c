#include "core.h"
#include <string.h>
#include <sys/stat.h>
#include "../include/utils.h"
#include "vm.h"
#include "../object/class.h"
#include "../object/obj_string.h"
#include "../object/obj_list.h"
#include "../object/obj_range.h"
#include "../compiler/compiler.h"
#include "../include/unicodeUtf8.h"
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
// 确认 value 是否为整数
static bool validateIntValue(VM* vm, double value){
    if(trunc(value) == value){
        return true;
    }
    SET_ERROR_FALSE(vm, "argument must be integer!");
}
// 检验 arg 是否是整数
static bool validateInt(VM* vm, Value arg){
    // 首先得是数字
    if(!validateNum(vm, arg)){
        return false;
    }
    // 再校验数值
    return validateIntValue(vm, VALUE_TO_NUM(arg));
}

// 检验参数 index 是否是落在 [0, length) 之间的整数
static uint32_t validateIndexValue(VM* vm, double index, uint32_t length){
    // 索引必须是数字
    if(!validateIntValue(vm, index)){
        return UINT32_MAX;
    }

    // 支持负数索引，负数是从后往前索引
    // 转换其应用的正数索引，如果校验失败则返回 UINT32_MAX
    if(index < 0){
        index += length;
    }

    // 索引应该落在 [0, length)
    if(index >= 0 && index < length){
        return (uint32_t)index;
    }

    // 执行到此说明超出范围
    vm->curThread->errorObj = OBJ_TO_VALUE(newObjString(vm, "index out of bound!", 19));
    return UINT32_MAX;
}

// 验证 index 有效性
static uint32_t validateIndex(VM* vm, Value index, uint32_t length){
    if(!validateNum(vm, index)){
        return UINT32_MAX;
    }
    return validateIndexValue(vm,VALUE_TO_NUM(index), length);
}

// 从码点 value 创建字符串
static Value makeStringFromCodePoint(VM* vm, int value){
    uint32_t byteNum = getByteNumOfEncodeUtf8(value);
    ASSERT(byteNum != 0, "utf8 encode bytes should be between 1 and 4!");

    // +1 是为了结尾的 \0
    ObjString* objString = ALLOCATE_EXTRA(vm, ObjString, byteNum + 1);

    if(objString == NULL){
        MEM_ERROR("allocate memory failed in runtime!");
    }

    initObjHeader(vm, &objString->objHeader, OT_STRING, vm->stringClass);
    objString->value.length = byteNum;
    objString->value.start[byteNum] = '\0';
    encodeUtf8((uint8_t*)objString->value.start, value);
    hashObjString(objString);
    return OBJ_TO_VALUE(objString);
}

// 用索引 index 处的字节创建字符串对象
static Value stringCodePointAt(VM* vm, ObjString* objString, uint32_t index){
    ASSERT(index < objString->value.length, "index out of bound!");
    int codePoint = decodeUtf8((uint8_t*)objString->value.start + index, objString->value.length - index);

    // 若不是有效的 UTF-8 序列，就将其处理为单个裸字符
    if(codePoint == -1){
        return OBJ_TO_VALUE(newObjString(vm, &objString->value.start[index], 1));
    }

    return makeStringFromCodePoint(vm, codePoint);
}

// 计算 objRange 中的元素的起始索引及索引方向
static uint32_t calculateRange(VM* vm, ObjRange* objRange, uint32_t* countPtr, int* directionPtr){
    uint32_t from = validateIndexValue(vm, objRange->from, *countPtr);
    if(from == UINT32_MAX){
        return UINT32_MAX;
    }

    uint32_t to = validateIndexValue(vm, objRange->to, *countPtr);
    if(to == UINT32_MAX){
        return UINT32_MAX;
    }

    // 如果 from 和 to 为负值，经过 validateIndexValue 已经变成了相应的正索引
    *directionPtr = from < to ? 1 : -1;
    *countPtr = abs((int)(from - to)) + 1;
    return from;
}

// 以 UTF-8 编码从 sourceStr 中起始为 startIndex， 方向为 direction 的 count 个字符创建字符串
static ObjString* newObjStringFromSub(VM* vm, ObjString* sourceStr, int startIndex, uint32_t count, int direction){
    uint8_t* source = (uint8_t*)sourceStr->value.start;
    uint32_t totalLength = 0, idx = 0;

    // 计算 count 个 UTF-8 编码的字符总共需要的字节数，后面好申请空间
    while(idx < count){
        totalLength += getByteNumOfDecodeUtf8(source[startIndex + idx * direction]);
        idx++;
    }

    // +1 是尾了结尾的\0
    ObjString* result = ALLOCATE_EXTRA(vm, ObjString, totalLength + 1);

    if(result == NULL){
        MEM_ERROR("allocate memory failed in runtime!");
    }
    initObjHeader(vm, &result->objHeader, OT_STRING, vm->stringClass);
    result->value.start[totalLength] = '\0';
    result->value.length = totalLength;

    uint8_t* dest = (uint8_t*)result->value.start;
    idx = 0;
    while(idx < count){
        int index = startIndex + idx * direction;
        // 解码， 获取字符数据
        int codePoint = decodeUtf8(source + index, sourceStr->value.length - index);
        if(codePoint != -1){
            // 再将数据按照 UTF-8 编码，写入 result
            dest += encodeUtf8(dest, codePoint);
        }
        idx++;
    }

    hashObjString(result);
    return result;
}

// 使用 Boyer-Moore-Horspool 字符串算法在 haystack 中查找 needle 大海捞针
static int findString(ObjString* haystack, ObjString* needle){
    // 如果查找的 pattern 为空则为找到
    if(needle->value.length == 0){
        return 0; // 返回起始下标0
    }

    // 若待搜索的字符串比原串还长，肯定搜不到
    if(needle->value.length > haystack->value.length){
        return -1;
    }

    // 构建 " bad-character shift" 表示确定窗口滑动的距离
    // 数组 shift 的值便是 滑动距离
    uint32_t shift[UINT8_MAX];
    // needle 中最后一个字符的下标
    uint32_t needleEnd = needle->value.length - 1;

    // 一、先假定 bad character 不属于 needle（即 pattern）
    // 对于这种情况，滑动窗口跨过整个 needle
    uint32_t idx = 0;
    while(idx < UINT8_MAX){
        // 默认为滑动整个 needle 的长度
        shift[idx] = needle->value.length;
        idx++;
    }

    // 二、假定 haystack 中与 needle 不匹配的字符在needle 中之前已经匹配过的位置出现过
    // 就滑动窗口以使该字符与needle中匹配该字符的最末位置对齐
    // 这里预先确定需要滑动的距离
    idx = 0;
    while(idx < needleEnd){
        char c = needle->value.start[idx];
        // idx  从前往后遍历 needle，当needle中有重复的字符c时，
        // 后面的字符c会覆盖的同名字符c，这保证了数组shift中字符是needle中最末位置的字符
        // 从而保证了 shift[c] 的值是 needle 中最末端
        shift[(uint8_t)c] = needleEnd - idx;
        idx++;
    }

    // Boyer-Moore-Horspool 是从后往前比较，这是处理 bad-charcter高效的地方
    // 因此获取 needle 中最后一个字符，用于同 haystack 的窗口中最后一个字符比较
    char lastChar = needle->value.start[needleEnd];

    // 长度差便是滑动窗口的滑动范围
    uint32_t range = haystack->value.length - needle->value.length;

    // 从hasystack中扫描 needle， 寻找第一个匹配的字符，如果遍历完了就停止
    idx = 0;
    while(idx <= range){
        // 拿 needle 中最后一个字符同 haystack 窗口的最后一个字符比较
        // 因为（Boyer-Moore-Horspool 是从后往前比较）如果皮皮鹅，看整个needle是否匹配
        char c = haystack->value.start[idx + needleEnd];
        if(lastChar == c && memcpy(haystack->value.start + idx, needle->value.start, needleEnd) == 0){
            // 找到了就返回匹配的位置
            return idx;
        }

        // 否则就向前滑动继续下一轮比较
        idx += shift[(uint8_t)c];
    }

    // 未找到就返回-1
    return -1;
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

// objString.fromCodePoint(): 从码点建立字符串
static bool primStringFromCodePoint(VM* vm, Value* args){
    if(!validateInt(vm, args[1])){
        return false;
    }

    int codePoint = (int)VALUE_TO_NUM(args[1]);
    if(codePoint < 0 ){
        SET_ERROR_FALSE(vm, "code point can't be negetive!");
    }

    if(codePoint > 0x10ffff){
        SET_ERROR_FALSE(vm, "code point must be between 0 and 0x10ffff!");
    }
    RET_VALUE(makeStringFromCodePoint(vm ,codePoint));
}
// objString  + objString : 字符串相加
static bool primStringPlus(VM* vm, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }

    ObjString* left = VALUE_TO_OBJSTR(args[0]);
    ObjString* right = VALUE_TO_OBJSTR(args[1]);
    uint32_t totalLength = strlen(left->value.start) + strlen(right->value.start);
    // +1 是为了结尾的 \0
    ObjString* result = ALLOCATE_EXTRA(vm, ObjString, totalLength + 1);
    if(result == NULL){
       MEM_ERROR("allocate memory failed in runtime!");
    }
    initObjHeader(vm, &result->objHeader, OT_STRING, vm->stringClass);
    memcpy(result->value.start, left->value.start, strlen(left->value.start));
    memcpy(result->value.start + strlen(left->value.start), right->value.start, strlen(right->value.start));
    result->value.start[totalLength] = '\0';
    result->value.length = totalLength;
    hashObjString(result);

    RET_OBJ(result);
}

// objString[_]: 用数字或objRange对象作字符串的 subScript
static bool primStringSubscript(VM* vm, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    // 数字和objRange都可以作索引，分别判断
    // 若索引是数字，就直接索引1个字符，这就是最简单的 subScript
    if(VALUE_IS_NULL(args[1])){
        uint32_t index = validateIndex(vm, args[1], objString->value.length);
        if(index == UINT32_MAX){
            return false;
        }
        RET_VALUE(stringCodePointAt(vm, objString, index));
    }

    // 索引要么为数字，要么 ObjRange， 若不是数字就应该为 objRange
    if(!VALUE_IS_OBJRANGE(args[1])){
        SET_ERROR_FALSE(vm, "subscript should be integer or range!");
    }

    // direction 是索引的方向
    // 1表示正方向，从前向后， -1表示反方向，从后向前
    // from 若比 to 大，即从后往前检索字符， direction则为-1
    int direction;

    uint32_t count = objString->value.length;
    // 返回的 startIndex 是objRange.from 在 objString.value.start中的下标
    uint32_t startIndex = calculateRange(vm, VALUE_TO_OBJRANGE(args[1]), &count, &direction);
    if(startIndex == UINT32_MAX){
        return false;
    }

    RET_OBJ(newObjStringFromSub(vm, objString, startIndex, count, direction));
}

// objString.byteAt_(): 返回指定索引的字节
static bool primStringByteAt(VM* vm UNUSED, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    uint32_t index = validateIndex(vm, args[1], objString->value.length);
    if(index == UINT32_MAX){
        return false;
    }
    // 故转换为数字返回
    RET_NUM((uint8_t)objString->value.start[index]);
}

// objString.byteCount_: 返回字节数
static bool primStringByteCount(VM* vm UNUSED, Value* args){
    RET_NUM(VALUE_TO_OBJSTR(args[0])->value.length);
}


// objString.codePointAt_(_): 返回指定的 CodePoint
static bool primStringCodePointAt(VM* vm UNUSED, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    uint32_t index = validateIndex(vm, args[1], objString->value.length);
    if(index == UINT32_MAX){
        return false;
    }

    const uint8_t* bytes = (uint8_t*)objString->value.start;
    if((bytes[index] & 0xc0) == 0x80){
        // 如果 index 指向的并不是 UTF-8 编码的最高字节
        // 而是后面的低字节，返回 -1 提示用户
        RET_NUM(-1);
    }

    // 返回解码
    RET_NUM(decodeUtf8((uint8_t*)objString->value.start + index, objString->value.length - index));
}

// objString.contains(_): 判断字符串 args[0] 中是否包含子字符串 args[1]
static bool primStringContains(VM* vm, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }

    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    ObjString* pattern = VALUE_TO_OBJSTR(args[1]);
    RET_BOOL(findString(objString, pattern) != -1);
}

// objString.endWith(_): 返回字符串是否以 args[1] 为结束
static bool primStringEndsWith(VM* vm UNUSED, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }

    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    ObjString* pattern = VALUE_TO_OBJSTR(args[1]);
    // 若pattern 比源串还长，源串必然不包括 pattern
    if(pattern->value.length > objString->value.length){
        RET_FALSE;
    }

    char* cmpIdx = objString->value.start + objString->value.length - pattern->value.length;
    RET_BOOL(memcmp(cmpIdx, pattern->value.start, pattern->value.length) == 0);
}

// objString.indexOf(_): 检索字符串 args[0] 中子串 args[1] 的起始下标
static bool primStringIndexOf(VM* vm UNUSED, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }

    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    ObjString* pattern = VALUE_TO_OBJSTR(args[1]);

    // 若 pattern 比源串还长，源串必然不包括 pattern
    if(pattern->value.length > objString->value.length){
        RET_FALSE;
    }

    int index = findString(objString, pattern);
    RET_NUM(index);
}

// objString.iterate(_): 返回下一个 UTF8字符 的迭代器
static bool primStringIterate(VM* vm UNUSED, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);

    // 如果是第一次迭代，迭代索引肯定为空
    if(VALUE_IS_NULL(args[1])){
        if(objString->value.length == 0){
            RET_FALSE;
        }
        RET_NUM(0);
    }
    // 迭代器必须是正整数
    if(!validateInt(vm, args[1])){
        return false;
    }
    double iter = VALUE_TO_NUM(args[1]);
    if(iter < 0){
        RET_FALSE;
    }

    uint32_t index = (uint32_t)iter;
    do{
        index++;

        // 到了结尾就返回false，表示迭代完毕
        if(index >= objString->value.length){
            RET_FALSE;
        }
        // 读取连续的数据字节，直到下一个 UTF-8 的高字节
    }while (objString->value.start[index] & 0xc0 == 0x80);

    RET_NUM(index);
}

// objString.iterateByte(_): 迭代索引，内部使用
static bool primStringIterateByte(VM* vm UNUSED, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    // 如果是第一次迭代，迭代索引肯定为空， 直接返回索引0
    if(VALUE_IS_NULL(args[1])){
        if(objString->value.length == 0){
            RET_FALSE;
        }
        RET_NUM(0);
    }

    // 迭代器必须是正整数
    if(!validateInt(vm, args[1])){
        return false;
    }

    double iter = VALUE_TO_NUM(args[1]);

    if(iter < 0){
        RET_FALSE;
    }

    uint32_t index = (uint32_t)iter;
    index++; // 移进下一个字节的索引
    if(index >= objString->value.length){
        RET_FALSE;
    }
    RET_NUM(index);
}

// objString.iteratorValue(_): 返回迭代器对应的value
static bool primStringIteratorValue(VM* vm, Value* args){
    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    uint32_t index = validateIndex(vm, args[1], objString->value.length);
    if(index == UINT32_MAX){
        return false;
    }

    RET_VALUE(stringCodePointAt(vm, objString, index));
}

// objString.startsWith(_): 返回 args[0] 是否以 args[1] 为起始
static bool primStringStartsWith(VM* vm UNUSED, Value* args){
    if(!validateString(vm, args[1])){
        return false;
    }

    ObjString* objString = VALUE_TO_OBJSTR(args[0]);
    ObjString* pattern = VALUE_TO_OBJSTR(args[1]);

    // 若 pattern 比源串还长， 源串必然不包括 pattern,
    // 因此不能以 pattern 为起始
    if(pattern->value.length > objString->value.length){
        RET_FALSE;
    }

    RET_BOOL(memcmp(objString->value.start, pattern->value.start, pattern->value.length) == 0);
}

// objString.toString: 获得自己的字符串
static bool primStringToString(VM* vm UNUSED, Value* args){
    RET_VALUE(args[0]);
}

// objList.new(): 创建一个新的 List
static bool primListNew(VM* vm, Value* args UNUSED){
    RET_OBJ(newObjList(vm, 0));
}

// objList[_]: 索引 list 元素
static bool primListSubscript(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);

    // 数字和 objRange都可以作索引，分别判断
    // 若索引是数字， 就直接索引1个字符，这是最简单的 subscript
    if(VALUE_IS_NUM(args[1])){
        uint32_t index = validateIndex(vm, args[1], objList->elements.count);
        if(index == UINT32_MAX){
            return false;
        }
        RET_VALUE(objList->elements.datas[index]);
    }

    // 索引要么为数字 要么为 ObjRange 若不是数字就应该为 objRange
    if(!VALUE_IS_OBJRANGE(args[1])){
        SET_ERROR_FALSE(vm, "subscript should bu integer or range!");
    }

    int direction;
    uint32_t count = objList->elements.count;

    // 返回的 startIndex 是 objRange.from 在 objList.elements.data 中的下标
    uint32_t startIndex = calculateRange(vm, VALUE_TO_OBJRANGE(args[1]), &count, &direction);

    // 新建一个 list，存储该 range 在原来 list 中索引的元素
    ObjList* result = newObjList(vm, count);
    uint32_t idx = 0;
    while(idx < count){
        // direction 为 -1 表示从后往前倒序赋值
        // 如 var a = [a,a,c,d,e,f,g]; a[5..3] 表示 [f,e,d]
        result->elements.datas[idx] = objList->elements.datas[startIndex + idx * direction];
        idx++;
    }
    RET_OBJ(result);
}

// objList[_]=(_) : 只支持数字作为 subscript
static bool primListSubscriptSetter(VM* vm UNUSED, Value* args){
    // 获取对象
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);

    // 获取sbuscript
    uint32_t index = validateIndex(vm, args[1], objList->elements.count);
    if(index == UINT32_MAX){
        return false;
    }

    // 直接赋值
    objList->elements.datas[index] = args[2];
    RET_VALUE(args[2]); // 把参数2作为返回值
}

// objList.add(_): 直接追加到list中
static bool primListAdd(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);
    ValueBufferAdd(vm, &objList->elements, args[1]);
    RET_VALUE(args[1]);  // 把参数1作为返回值
}

// objList.addCore_(_): 编译内部使用，用于编译列表直接量
static bool primListAddCore(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);
    ValueBufferAdd(vm, &objList->elements, args[1]);
    RET_VALUE(args[0]); // 返回列表自身
}

// objList.clear(): 清空了list
static bool primListClear(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);
    ValueBufferClear(vm, &objList->elements);
    RET_NULL;
}

// objList.cunt: 返回list中元素个数
static bool primListCount(VM* vm UNUSED, Value* args){
    RET_NUM(VALUE_TO_OBJLIST(args[0])->elements.count);
}

// objList.insert(_,_): 插入元素
static bool primListInsert(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);
    // +1 确保可以在最后插入
    uint32_t index = validateIndex(vm, args[1], objList->elements.count + 1);
    if(index == UINT32_MAX){
        return false;
    }
    insertElement(vm, objList, index, args[2]);
    RET_VALUE(args[2]); // 参数2作为返回值
}

// objList.iterate(): 迭代list
static bool primListIterate(VM* vm, Value* args){
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);

    // 如果是第一次迭代， 迭代索引肯定为空，直接返回索引0
    if(VALUE_IS_NULL(args[1])){
        if(objList->elements.count == 0){
            RET_FALSE;
        }
        RET_NUM(0);
    }

    // 确保迭代器是整数
    if(!validateInt(vm, args[1])){
       return false;
    }

    double iter = VALUE_TO_NUM(args[1]);
    // 如果迭代完了就终止
    if(iter<0 || iter >= objList->elements.count -1 ){
        RET_FALSE;
    }

    RET_NUM(iter + 1); // 返回下一个
}

// objList.iteratorValue(_): 返回迭代值
static bool primListIteratorValue(VM* vm, Value* args){
    // 获取实例对象
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);

    uint32_t index = validateIndex(vm, args[1], objList->elements.count);
    if(index == UINT32_MAX){
        return false;
    }
    RET_VALUE(objList->elements.datas[index]);
}

// objList.removeAt(_): 删除指定位置的元素
static bool primListRemoveAt(VM* vm, Value* args){
    // 获取实例对象
    ObjList* objList = VALUE_TO_OBJLIST(args[0]);

    uint32_t index = validateIndex(vm, args[1], objList->elements.count);
    if(index == UINT32_MAX){
        return false;
    }

    RET_VALUE(removeElement(vm, objList, index));
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
    PRIM_METHOD_BIND(vm->numClass, "+(_)", primNumPlus);
    PRIM_METHOD_BIND(vm->numClass, "-(_)", primNumMinus);
    PRIM_METHOD_BIND(vm->numClass, "*(_)", primNumMul);
    PRIM_METHOD_BIND(vm->numClass, "/(_)", primNumDiv);
    PRIM_METHOD_BIND(vm->numClass, ">(_)", primNumGt);
    PRIM_METHOD_BIND(vm->numClass, ">=(_)", primNumGe);
    PRIM_METHOD_BIND(vm->numClass, "<(_)", primNumLt);
    PRIM_METHOD_BIND(vm->numClass, "<=(_)", primNumLe);

    // 位运算
    PRIM_METHOD_BIND(vm->numClass, "&(_)", primNumBitAnd);
    PRIM_METHOD_BIND(vm->numClass, "|(_)", primNumBitOr);
    PRIM_METHOD_BIND(vm->numClass, ">>(_)", primNumBitShiftRight);
    PRIM_METHOD_BIND(vm->numClass, "<<(_)", primNumBitShiftLeft);

    // 以上都是通过 rules 中 INFIX_OPERATOR 解析
    // 少数符号依然是 INFIX_OPERATOR 解析
    PRIM_METHOD_BIND(vm->numClass, "abs", primNumAbs);
    PRIM_METHOD_BIND(vm->numClass, "acos", primNumAcos);
    PRIM_METHOD_BIND(vm->numClass, "asin", primNumAsin);
    PRIM_METHOD_BIND(vm->numClass, "atan", primNumAtan);
    PRIM_METHOD_BIND(vm->numClass, "ceil", primNumCeil);
    PRIM_METHOD_BIND(vm->numClass, "cos", primNumCos);
    PRIM_METHOD_BIND(vm->numClass, "floor", primNumFloor);
    PRIM_METHOD_BIND(vm->numClass, "-", primNumNegate);
    PRIM_METHOD_BIND(vm->numClass, "sin", primNumSin);
    PRIM_METHOD_BIND(vm->numClass, "sqrt", primNumSqrt);
    PRIM_METHOD_BIND(vm->numClass, "tan", primNumTan);
    PRIM_METHOD_BIND(vm->numClass, "%(_)", primNumMod);
    PRIM_METHOD_BIND(vm->numClass, "~", primNumBitNot);
    PRIM_METHOD_BIND(vm->numClass, "..(_)", primNumRange);
    PRIM_METHOD_BIND(vm->numClass, "atan(_)", primNumAtan2);
    PRIM_METHOD_BIND(vm->numClass, "fraction", primNumFraction);
    PRIM_METHOD_BIND(vm->numClass, "isInfinity", primNumIsInfinity);
    PRIM_METHOD_BIND(vm->numClass, "isInteger", primNumIsInteger);
    PRIM_METHOD_BIND(vm->numClass, "isNan", primNumIsNan);
    PRIM_METHOD_BIND(vm->numClass, "toString", primNumToString);
    PRIM_METHOD_BIND(vm->numClass, "==(_)", primNumEqual);
    PRIM_METHOD_BIND(vm->numClass, "!=(_)", primNumNotEqual);

    // 字符串类
    vm->stringClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "String"));
    // 类方法
    PRIM_METHOD_BIND(vm->stringClass->objHeader.class, "fromCodePoint(_)", primStringFromCodePoint);
    // 实例方法
    PRIM_METHOD_BIND(vm->stringClass, "+(_)", primStringPlus);
    PRIM_METHOD_BIND(vm->stringClass, "[_]", primStringSubscript);
    PRIM_METHOD_BIND(vm->stringClass, "byteAt_(_)", primStringByteAt);
    PRIM_METHOD_BIND(vm->stringClass, "byteCount_", primStringByteCount);
    PRIM_METHOD_BIND(vm->stringClass, "codePointAt_(_)", primStringCodePointAt);
    PRIM_METHOD_BIND(vm->stringClass, "contains(_)", primStringContains);
    PRIM_METHOD_BIND(vm->stringClass, "endsWith(_)", primStringEndsWith);
    PRIM_METHOD_BIND(vm->stringClass, "indexOf(_)", primStringIndexOf);
    PRIM_METHOD_BIND(vm->stringClass, "iterate(_)", primStringIterate);
    PRIM_METHOD_BIND(vm->stringClass, "iterateByte_(_)", primStringIterateByte);
    PRIM_METHOD_BIND(vm->stringClass, "iteratorValue(_)", primStringIteratorValue);
    PRIM_METHOD_BIND(vm->stringClass, "startsWith(_)", primStringStartsWith);
    PRIM_METHOD_BIND(vm->stringClass, "toString", primStringToString);
    PRIM_METHOD_BIND(vm->stringClass, "count", primStringByteCount);

    // List 类
    vm->listClass = VALUE_TO_CLASS(getCoreClassValue(coreModule, "List"));
    // 类方法
    PRIM_METHOD_BIND(vm->listClass->objHeader.class, "new()", primListNew);
    // 实例方法
    PRIM_METHOD_BIND(vm->listClass, "[_]", primListSubscript);
    PRIM_METHOD_BIND(vm->listClass, "[_]=(_)", primListSubscriptSetter);
    PRIM_METHOD_BIND(vm->listClass, "add(_)", primListAdd);
    PRIM_METHOD_BIND(vm->listClass, "addCore(_)", primListAddCore);
    PRIM_METHOD_BIND(vm->listClass, "clear()", primListClear);
    PRIM_METHOD_BIND(vm->listClass, "count", primListCount);
    PRIM_METHOD_BIND(vm->listClass, "insert(_,_)", primListInsert);
    PRIM_METHOD_BIND(vm->listClass, "iterate(_)", primListIterate);
    PRIM_METHOD_BIND(vm->listClass, "iteratorValue(_)", primListIteratorValue);
    PRIM_METHOD_BIND(vm->listClass, "removeAt(_)", primListRemoveAt);
}
