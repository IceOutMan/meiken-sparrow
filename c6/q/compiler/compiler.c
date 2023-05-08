#include <string.h>
#include "compiler.h"
#include "../parser/parser.h"
#include "../vm/core.h"
#include "../include/utils.h"
#include "../object/obj_fn.h"

#if DEBUG
#include "debug.h"
#endif

// 函数、方法 等独立的指令流都是编译单元
struct compileUnit {
    // 所编译的函数（这里叫做函数，其实ObjFn表示的是代码指令单元
    ObjFn *fn;

    // 作用域中允许的局部变量的个数上限
    LocalVar localVars[MAX_LOCAL_VAR_NUM];

    // 已分配的局部变量个数
    uint32_t localVarNum;

    // 记录本层函数所引用的 upvalue
    Upvalue upvalues[MAX_UPVALUE_NUM];

    // 此项百事当前正在编译的代码所处的作用域
    int scopeDepth;

    // 当前使用的slot个数
    uint32_t stackSlotNum;

    // 当前正在编译的循环层
    Loop *curLoop;

    // 当前正在编译的类的编译信息
    ClassBookKeep *enclosingClassBK;

    // 包含此编译单元的编译单元，既直接外层
    struct compileUnit *enclosingUnit;

    // 当前parser
    Parser *curParser;
}; // 编译单元

typedef enum {
    VAR_SCOPE_INVALID,
    VAR_SCOPE_LOCAL,    // 局部变量
    VAR_SCOPE_UPVALUE,  // upvalue
    VAR_SCOPE_MODULE    // 模块变量
} VarScopeType; // 标识变量作用域

typedef struct {
    VarScopeType scopeType; // 变量的作用域
    // 根据 scodeType 的值
    // 此索引可能指向局部变量或upvalue或模块变量
    int index;
} Variable;

// 枚举值越大，绑定权值越大
typedef enum {
    BP_NONE,        // 无绑定能力
    BP_LOWEST,      // 最低绑定能力
    BP_ASSIGN,      // =
    BP_CONDITION,   // 三目运算符 ?:
    BP_LOGIC_OR,    // ||
    BP_LOGIC_AND,   // &&
    BP_EQUAL,       // = !=
    BP_IS,          // is
    BP_CMP,         // < > <= >=
    BP_BIT_OR,      // |
    BP_BIT_AND,     // &
    BP_BIT_SHIFT,   // << >>
    BP_RANGE,       // ..
    BP_TERM,        // + -
    BP_FACTOR,      // * / %
    BP_UNARY,       // - ! ~
    BP_CALL,        // . () []
    BP_HIGHEST
} BindPower; // 定义了操作符的绑定权值, 既优先级

// 指示符函数指针
typedef void (*DenotationFn)(CompileUnit *cu, bool canAssign);

// 签名函数指针
typedef void (*methodSignatureFn)(CompileUnit *cu, Signature *signature);

// 符号绑定规则
typedef struct {
    const char *id; // 符号

    // 左绑定权值， 不关注左边操作数的符号此值为0
    BindPower lbp;

    // 字面量、变量、前缀运算符等不关注左操作数的Token调用的方法
    DenotationFn nud;

    // 中缀运算符等关注左操作数的Token调用的方法
    DenotationFn led;

    // 表示本符号在类中被视为一个方法，为其生成一个方法签名
    methodSignatureFn methodSign;
} SymbolBindRule; // 符号绑定规则


// 方法声明
static void initCompileUnit(Parser *parser, CompileUnit *cu, CompileUnit *enclosingUnit, bool isMethod);
static uint32_t addConstant(CompileUnit *cu, Value constant);
static uint32_t addLocalVar(CompileUnit *cu, const char *name, uint32_t length);
static int addUpvalue(CompileUnit *cu, bool isEnclosingLocalVar, uint32_t index);

static void emitCall(CompileUnit* cu, int numArgs, const char* name, int length);

static void compileBody(CompileUnit *cu, bool isConstruct);
static void compileProgram(CompileUnit *cu);
static ObjFn *endCompileUnit(CompileUnit *cu);
static void compileVarDefinition(CompileUnit *cu, bool isStatic);
static void compileStatement(CompileUnit* cu);
static void compileWhileStatement(CompileUnit* cu);
inline static void compileReturn(CompileUnit* cu);
inline static void compileBreak(CompileUnit* cu);
inline static void compileContinue(CompileUnit* cu);
static void compileLoopBody(CompileUnit* cu);
static void compileBlock(CompileUnit *cu);

static void enterLoopSetting(CompileUnit* cu, Loop* loop);
static void leaveLoopPath(CompileUnit* cu);


static void processParaList(CompileUnit *cu, Signature *sign);
static void processArgList(CompileUnit *cu, Signature *sign);
static Variable getVarFromLocalOrUpvalue(CompileUnit *cu, const char *name, uint32_t length);


static void literal(CompileUnit *cu, bool canAssign UNUSED);
static void expression(CompileUnit *cu, BindPower rbp);
static void id(CompileUnit *cu, bool canAssign);
static void stringInterpolation(CompileUnit *cu, bool canAssign UNUSED);
static void idMethodSignature(CompileUnit *cu, Signature *sign);
static void infixMethodSignature(CompileUnit *cu, Signature *sign);
static void infixOperator(CompileUnit *cu, bool canAssign UNUSED);
static void unaryOperator(CompileUnit *cu, bool canAssign UNUSED);
static void boolean(CompileUnit *cu, bool canAssign UNUSED);
static void null(CompileUnit *cu, bool canAssign UNUSED);
static void this(CompileUnit *cu, bool canAssign UNUSED);
static void super(CompileUnit *cu, bool canAssign);
static void parentheses(CompileUnit *cu, bool canAssign UNUSED);
static void listLiteral(CompileUnit *cu, bool canAssign UNUSED);
static void subscript(CompileUnit *cu, bool canAssign);
static void subscriptMethodSignature(CompileUnit *cu, Signature *sign);
static void callEntry(CompileUnit *cu, bool canAssign);
static void mapLiteral(CompileUnit *cu, bool canAssign UNUSED);
static uint32_t emitInstrWithPlaceholder(CompileUnit *cu, OpCode opCode);
static void patchPlaceholder(CompileUnit *cu, uint32_t absIndex);
static void logicOr(CompileUnit *cu, bool canAssign UNUSED);
static void logicAnd(CompileUnit *cu, bool canAssign UNUSED);
static void condition(CompileUnit *cu, bool canAssign UNUSED);

static void defineVariable(CompileUnit *cu, uint32_t index);
static Variable findVariable(CompileUnit *cu, const char *name, uint32_t length);
static uint32_t discardLocalVar(CompileUnit* cu, int scopeDepth);






// 不关注左操作数的符号称为前缀符号
// 用于如字面量、变量名、前缀符号等非运算符
#define PREFIX_SYMBOL(nud) {NULL, BP_NONE, nud, NULL, NULL}

// 前缀运算符，如 !
#define PREFIX_OPERATION(id) {id, BP_NONE, unaryOperator, NULL, unaryMethodSignature}

// 关注左操作数的符号称为中缀符号
// 数组[, 函数(, 实例与方法之间的. 等
#define INFIX_SYMBOL(lbp, led) {NULL, lbp, NULL, led, NULL}

// 中缀运算符
#define INFIX_OPERATOR(id, lbp) {id, lbp, NULL, infixOperator, infixMethodSignature}

// 既可作前缀又可作中缀的运算符， 如-
#define  MIX_OPERATOR(id) {id, BP_TERM, unaryOperator, infixOperator, mixMethodSignature}

// 占位使用
#define UNUSED_RULE {NULL, BP_NONE, NULL, NULL, NULL}


// 把opcode定义到数组 opCodeSlotsUsed中, #define 后面使用逗号，作为参数使用该宏
#define OPCODE_SLOTS(opCode, effect) effect,
static const int opCodeSlotsUsed[] = {
#include "../vm/opcode.inc"
};
#undef OPCODE_SLOTS

// 往函数的指令流中写入1字节，返回其索引
static int writeByte(CompileUnit *cu, int byte) {
    // 若在调试状态，额外在 debug->lineNo中写入当前token行号
#if DEBUG
    IntbufferAdd(cu->curParser->vm, &cu->fn->debug->lineNo, cu->curParser->preToken.lineNo);
#endif
    ByteBufferAdd(cu->curParser->vm, &cu->fn->instrStream, (uint8_t) byte);
    return cu->fn->instrStream.count - 1;
}


// 把Signature转换为字符串，返回字符串长度
static uint32_t sign2String(Signature *sign, char *buf) {
    uint32_t pos = 0;
    // 复制方法名 XXX
    memcpy(buf + pos, sign->name, sign->length);
    pos += sign->length;

    // 下面单独处理方法名之后的部分
    switch (sign->type) {
        // SIGN_GETTER形式：xxx，无参数，上面 memcpy 已完成
        case SIGN_GETTER:
            break;

            // SIGN_SETTER形式：xxx=(_), 之前已完成XXXX
        case SIGN_SETTER:
            buf[pos++] = '=';
            // 下面添加=右边的赋值，只支持一个赋值
            buf[pos++] = '(';
            buf[pos++] = '_';
            buf[pos++] = ')';
            break;
            // SIGN_METHOD 和 SIGN_CONSTRUCT形式：xxx(_,...)
        case SIGN_CONSTRUCT:
        case SIGN_METHOD:
            buf[pos++] = '(';
            uint32_t idx = 0;
            while (idx < sign->argNum) {
                buf[pos++] = '_';
                buf[pos++] = ',';
                idx++;
            }
            if (idx == 0) { // 说明没有参数
                buf[pos++] = ')';
            } else {
                // 用 rightBracket 覆盖最后的
                buf[pos - 1] = ')';
            }
            break;
            // SIGN_SUBSCRIPT 形式 xxxx[_, ...]
        case SIGN_SUBSCRIPT: {
            buf[pos++] = '[';
            uint32_t idx = 0;
            while (idx < sign->argNum) {
                buf[pos++] = '_';
                buf[pos++] = ',';
                idx++;
            }
            if (idx == 0) {
                // 说明没有参数
                buf[pos++] = ']';
            } else {
                // 使用 rightBracket 覆盖最后
                buf[pos - 1] = ']';
            }
            break;
        }
            // SIGN_SUBSCRIPT_SETTER 形式：xxx[_, ... ] = (_)
        case SIGN_SUBSCRIPT_SETTER: {
            buf[pos++] = '[';
            uint32_t idx = 0;
            // argNum 包括了等号右边的一个赋值参数
            // 这里是在处理等号左边subscript 中的参数列表，因此减1
            // 后面专门添加该参数
            while (idx < sign->argNum - 1) {
                buf[pos++] = '_';
                buf[pos++] = ',';
                idx++;
            }
            if (idx == 0) {
                // 说明没有参数
                buf[pos++] = ']';
            } else {
                // 使用 rightBracket 覆盖最后的
                buf[pos - 1] = ']';
            }

            // 下面为等号右边的参数构造签名部分
            buf[pos++] = '=';
            buf[pos++] = '(';
            buf[pos++] = '_';
            buf[pos++] = ')';
        }
    }

    buf[pos] = '\0';
    return pos;  // 返回签名串的长度
}

// 写入操作码
static void writeOpCode(CompileUnit *cu, OpCode opCode) {
    writeByte(cu, opCode);
    // 累计需要的运行时空间大小
    cu->stackSlotNum += opCodeSlotsUsed[opCode];
    if (cu->stackSlotNum > cu->fn->maxStackSlotUsedNum) {
        cu->fn->maxStackSlotUsedNum = cu->stackSlotNum;
    }
}

// 写入1个字节的操作数
static int writeByteOperand(CompileUnit *cu, int operand) {
    return writeByte(cu, operand);
}

// 写入两字节的操作数 按大端字节序列写入参数，低地址写高位，高地址写低位
inline static void writeShortOperand(CompileUnit *cu, int operand) {
    writeByte(cu, (operand >> 8) & 0xff); // 先写高8位
    writeByte(cu, operand & 0xff); // 再写低8位
}

// 写入操作数为1字节大小的指令
static int writeOpCodeByteOperand(CompileUnit *cu, OpCode opCode, int operand) {
    writeOpCode(cu, opCode);
    return writeByteOperand(cu, operand);
}

// 写入操作数为2字节大小的指令
static void writeOpCodeShortOperand(CompileUnit *cu, OpCode opCode, int operand) {
    writeOpCode(cu, opCode);
    writeShortOperand(cu, operand);
}


// 添加常量并返回其索引
static uint32_t addConstant(CompileUnit *cu, Value constant) {
    ValueBufferAdd(cu->curParser->vm, &cu->fn->constants, constant);
    return cu->fn->constants.count - 1;
}

// 添加到局部变量cu
static uint32_t addLocalVar(CompileUnit *cu, const char *name, uint32_t length) {
    LocalVar *var = &(cu->localVars[cu->localVarNum]); // 取出最新的一个局部变量数组中的占位
    var->name = name;
    var->length = length;
    var->scopeDepth = cu->scopeDepth;
    var->isUpvalue = false;
    return cu->localVarNum++; // locaVarNum 类似当前编译单元的局部变量数组的游标
}

// 添加upvalue 到cu->upvalues， 返回其索引，若已存在则只返回索引
static int addUpvalue(CompileUnit *cu, bool isEnclosingLocalVar, uint32_t index) {
    uint32_t idx = 0;
    while (idx < cu->fn->upvalueNum) {
        // 如果该upvalue 已经添加过了就返回其索引
        if (cu->upvalues[idx].index == index && cu->upvalues[idx].isEnclosingLocalVar == isEnclosingLocalVar) {
            return idx;
        }
        idx++;
    }
    // 若没有找到则将其添加
    cu->upvalues[cu->fn->upvalueNum].isEnclosingLocalVar = isEnclosingLocalVar;
    cu->upvalues[cu->fn->upvalueNum].index = index; // 外层函数中局部变量的索引或者外层函数中upvalue的索引
    return cu->fn->upvalueNum++;
}

// 进入新作用域
static void enterScope(CompileUnit* cu){
    // 进入内嵌作用域
    cu->scopeDepth++;
}

// 退出作用域
static void leaveScope(CompileUnit* cu){
    // 对于非模块编译单元，丢弃局部变量
    if(cu->enclosingUnit != NULL){
        // 出作用域后丢弃本作用域以内的局部变量
        uint32_t discardNum = discardLocalVar(cu, cu->scopeDepth);
        cu->localVarNum -= discardNum;
        cu->stackSlotNum -= discardNum;
    }

    // 回到上一层作用域
    cu->scopeDepth--;
}

// 编译for 循环， 如for i (sequence) {循环体}
static void compileForStatement(CompileUnit* cu){
    // 其中 sequence 是可迭代的序列
    // for 循环会按照 while 循环的逻辑编译
//    for i (sequence){
//        System.print(i)
//    }

// 在内部会变成
// var seq = sequence
// var iter
// while iter = seq.iterate(iter){
//      var i = seq.iteratorValue(iter)
//      System.print(i)
//  }

    // 为局部变量seq 和 iter 创建作用域
    enterScope(cu);

    // 读取循环变量的名字， 如 for i(sequence) 中 i
    consumeCurToken(cu->curParser, TOKEN_ID, "expect variable after for!");
    const char* loopVarName = cu->curParser->preToken.start;
    uint32_t loopVarLen = cu->curParser->preToken.length;

    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' before sequence!");

    // 编译迭代序列
    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after sequence!");
    // 申请一个局部变量seq来存储序列对象
    // 其值就是上面 expression 存储到栈中的结果
    uint32_t seqSlot = addLocalVar(cu, "seq", 4);

    writeOpCode(cu, OPCODE_PUSH_NULL);
    // 分配及初始化 iter， 其值就是上面加载到栈中的 NULL
    uint32_t iterSlot = addLocalVar(cu, "iter ", 5);

    Loop loop;
    enterLoopSetting(cu, &loop);

    // 为调用 seq.iterate(iter) 做准备
    // 1. 先压入序列对象 seq， 既 seq.iterate(iter) 中的seq
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, seqSlot);
    // 2. 再压入参数 iter， 即 seq.iterate(iter) 中的 iter
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, iterSlot);
    // 3. 调用 seq.iterate(iter)
    emitCall(cu ,1, "iterate(_)", 10);
    // seq.iterate(iter) 把结果（下一个迭代器）存储到 args[0](即栈顶)， 现在将其同步到变量 iter
    writeOpCodeByteOperand(cu, OPCODE_STORE_LOCAL_VAR, iterSlot);

    // 如果条件失败则跳出循环体， 目前不知道循环体的结束地址
    // 先写入占位符
    loop.exitIndex = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 调用 seq.iteratorValue(iter) 以获取值
    // 1. 为调用 seq.iteratorValue(iter) 压入参数 seq
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, seqSlot);

    // 2. 为调用 seq.iteratorValue(iter) 压入参数 iter
    writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, iterSlot);

    // 3. 调用 seq.iteratorValue(iter)
    emitCall(cu, 1, "iteratorValue(_)", 16);

    // 为循环变量 i 创建作用域
    enterScope(cu);
    // seq.iteratorValue(iter) 已经把结果存储到栈顶
    // 添加循环变量为局部变量，其值在栈顶
    addLocalVar(cu, loopVarName, loopVarLen);

    // 编译循环体
    compileLoopBody(cu);

    leaveScope(cu); // 离开循环变量 i 的作用域

    leaveLoopPath(cu);

    leaveScope(cu); // 离开变量 seq 和 iter 的作用域
}



// 生成加载常量的指令
static void emitLoadConstant(CompileUnit *cu, Value value) {
    int index = addConstant(cu, value);
    writeOpCodeShortOperand(cu, OPCODE_LOAD_CONSTANT, index);
}


// 通过签名编译方法调用，包括callX 和 superX 指令
static void emitCallBySignature(CompileUnit *cu, Signature *sign, OpCode opcode) {
    char signBuffer[MAX_SIGN_LEN];
    uint32_t length = sign2String(sign, signBuffer);

    // 确保签名录入到 vm->allMethodNames 中
    int symbolIndex = ensureSymbolExist(cu->curParser->vm, &cu->curParser->vm->allMethodNames, signBuffer, length);
    writeOpCodeShortOperand(cu, opcode + sign->argNum, symbolIndex);

    // 此时在常量表中预创建一个空slot占位，将来绑定方法时再装入基类
    if (opcode == OPCODE_SUPER0) {
        writeShortOperand(cu, addConstant(cu, VT_TO_VALUE(VT_NULL)));
    }
}

// 生成方法调用的指令，仅限 callX 指令
static void emitCall(CompileUnit *cu, int numArgs, const char *name, int length) {
    int symbolIndex = ensureSymbolExist(cu->curParser->vm, &cu->curParser->vm->allMethodNames, name, length);
    writeOpCodeShortOperand(cu, OPCODE_CALL0 + numArgs, symbolIndex);
}

// 生成把变量 var 加载到栈的指令
static void emitLoadVariable(CompileUnit *cu, Variable var) {
    switch (var.scopeType) {
        case VAR_SCOPE_LOCAL:
            // 生成加载局部变量入栈的指令
            writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, var.index);
            break;
        case VAR_SCOPE_UPVALUE:
            // 生成加载upvalue到栈的指令
            writeOpCodeByteOperand(cu, OPCODE_LOAD_UPVALUE, var.index);
            break;
        case VAR_SCOPE_MODULE:
            // 生成加载模块变量到栈的指令
            writeOpCodeShortOperand(cu, OPCODE_LOAD_MODULE_VAR, var.index);
            break;
        default:
            NOT_REACHED();
    }
}

// 为变量var生成存储的指令
static void emitStoreVariable(CompileUnit *cu, Variable var) {
    switch (var.scopeType) {
        case VAR_SCOPE_LOCAL:
            // 生成存储局部变量的指令
            writeOpCodeByteOperand(cu, OPCODE_STORE_LOCAL_VAR, var.index);
            break;
        case VAR_SCOPE_UPVALUE:
            // 生成存储 upvalue 的指令
            writeOpCodeByteOperand(cu, OPCODE_STORE_UPVALUE, var.index);
            break;
        case VAR_SCOPE_MODULE:
            // 生成存储模块变量的指令
            writeOpCodeByteOperand(cu, OPCODE_STORE_MODULE_VAR, var.index);
            break;
        default:
            NOT_REACHED();

    }
}

// 生成加载或存储变量的指令
static void emitLoadOrStoreVariable(CompileUnit *cu, bool canAssign, Variable var) {
    if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
        expression(cu, BP_LOWEST); // 计算=右表达式的值
        emitStoreVariable(cu, var); // 为var生成赋值指令
    } else {
        emitLoadVariable(cu, var); // 为var生成读取指令
    }
}

// 生成 getter 或一般method调用指令
static void emitGetterMethodCall(CompileUnit *cu, Signature *sign, OpCode opCode) {
    Signature newSign;
    newSign.type = SIGN_GETTER; // 默认为getter，假设下面的两个if不执行
    newSign.name = sign->name;
    newSign.length = sign->length;
    newSign.argNum = 0;

    // 如果是method，有可能又参数列表，在生成调用方法的指令前必须把参数入栈，否则运行方法时
    // 除了会取到错误的参数（即栈中已有的数据）外，g还会在从方法返回时，错误地回收参数空间而导致栈失衡

    // 下面调用的 processArgList 是把实参入栈，供方法使用
    if (matchToken(cu->curParser, TOKEN_LEFT_PAREN)) { // 判断后面是否有(
        newSign.type = SIGN_METHOD;
        // 若后面不是) 说明有参数列表
        if (!matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            processArgList(cu, &newSign);
            consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "exepct ')' after argument list!");
        }
    }

    // 对method来说可能还传入了块参数
    if (matchToken(cu->curParser, TOKEN_LEFT_BRACE)) {
        newSign.argNum++;
        // 进入本if块时，上面的if块未必执行过
        // 此时 newSign.type 也许还是 GETTER，下面要将其设置为METHOD
        newSign.type = SIGN_METHOD;

        CompileUnit fnCU;
        initCompileUnit(cu->curParser, &fnCU, cu, false);

        Signature tmpFnSign = {SIGN_METHOD, "", 0, 0}; // 临时用于编译函数
        if (matchToken(cu->curParser, TOKEN_BIT_OR)) {    // 若块参数也有参数
            processParaList(&fnCU, &tmpFnSign);
            consumeCurToken(cu->curParser, TOKEN_BIT_OR, "expect '|' after argument list!");
        }
        fnCU.fn->argNum = tmpFnSign.argNum;

        // 编译函数体，将指令流写进该函数自己的指令单元fnCU
        compileBody(&fnCU, false);
#if DEBUG
        // 以此函数被传给的方法来命名这个函数， 函数名=方法名+" block arg"
        char fnName[MAX_SIGN_LEN + 10] = {"\0"}; // "block arg\0"
        uint32_t len = sign2String(&newSign, fnName);
        memmove(fnName + len, " block arg", 10);
        endCompileUnit(&fnCU, fnName, len+10);
#else
        endCompileUnit(&fnCU);
#endif
    }

    // 如果是在构造函数中调用了 super 则会执行到此，构造函数中调用的方法只能是super
    if (sign->type == SIGN_CONSTRUCT) {
        if (newSign.type != SIGN_METHOD) {
            COMPILE_ERROR(cu->curParser, "the form of supercall is suerp() or super(arguments)");
        }
        newSign.type = SIGN_CONSTRUCT;
    }

    // 根据签名生成调用的指令，如果上面的三个if都未执行，此处就是getter调用
    emitCallBySignature(cu, &newSign, opCode);
}

// 生成方法调用指令，报错 getter 和 setter
static void emitMethodCall(CompileUnit *cu, const char *name, uint32_t length, OpCode opCode, bool canAssign) {
    Signature sign;
    sign.type = SIGN_GETTER;
    sign.name = name;
    sign.length = length;

    // 若是 setter 则 生成调用 setter 的指令
    if (matchToken(cu->curParser, TOKEN_ASSIGN) && canAssign) {
        sign.type = SIGN_SETTER;
        sign.argNum = 1; // setter 只接受一个参数
        expression(cu, BP_LOWEST);

        emitCallBySignature(cu, &sign, opCode);
    } else {
        emitGetterMethodCall(cu, &sign, opCode);
    }
}

// 生成加载类的指令
static void emitLoadModuleVar(CompileUnit *cu, const char *name) {
    int index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, name, strlen(name));
    ASSERT(index != -1, "symbol should have been defined!");
    writeOpCodeByteOperand(cu, OPCODE_LOAD_MODULE_VAR, index);
}

// 生成把实例对象this加载到栈的指令
static void emitLoadThis(CompileUnit *cu) {
    // 找到变量 this
    Variable var = getVarFromLocalOrUpvalue(cu, "this", 4);
    ASSERT(var.scopeType != VAR_SCOPE_INVALID, "get variable failed!");
    emitLoadVariable(cu, var);
}


// 初始化 CompileUnit
static void initCompileUnit(Parser *parser, CompileUnit *cu,
                            CompileUnit *enclosingUnit, bool isMethod) {
    parser->curCompileUnit = cu;
    cu->curParser = parser;
    cu->enclosingUnit = enclosingUnit;
    cu->curLoop = NULL;
    cu->enclosingClassBK = NULL;

    // 若没有外层，说明当前属于模块作用域
    if (enclosingUnit == NULL) {
        // 编译代码时是从上到下从最外层的模块作用域开始，模块作用域设为-1
        cu->scopeDepth = -1;
        // 模块级作用域中没有局部变量
        cu->localVarNum = 0;
    } else {
        // 若是内层单元，属局部作用域 , isMethod 为true表示方法（属于类），否则表示函数（属于对象）
        if (isMethod) {
            // 若是类中的方法
            // 如果是类中的方法就设定隐式this为第0个局部变量，即实例对象
            // 它是方法（消息）的接收者，this这种特殊对象被处理为局部变量
            cu->localVars[0].name = "this";
            cu->localVars[0].length = 4;
        } else {
            // 若为普通函数
            // 空出第0个局部变量，保持统一
            cu->localVars[0].name = NULL;
            cu->localVars[0].length = 0;
        }
        // 第0个局部变量的特殊性使其作用域为模块级别
        cu->localVars[0].scopeDepth = -1;
        cu->localVars[0].isUpvalue = false;
        cu->localVarNum = 1; // localVars[0]被分配
        // 对于函数和方法来说，初始作用域是局部作用域
        // 0 表示局部作用域的最外层
        cu->scopeDepth = 0;
    }
    // 局部变量保存在栈中，初始时栈中已使用的 slot 数量等于局部变量的数量
    cu->stackSlotNum = cu->localVarNum;

    cu->fn = newObjFn(cu->curParser->vm, cu->curParser->curModule, cu->localVarNum);
}


// 声明局部变量
static int declareLocalVar(CompileUnit *cu, const char *name, uint32_t length) {
    if (cu->localVarNum >= MAX_LOCAL_VAR_NUM) {
        COMPILE_ERROR(cu->curParser, "the max length of local varibale of one scope is %d", MAX_LOCAL_VAR_NUM);
    }
    // 判断当前作用域中该变量是否已存在
    int idx = (int) cu->localVarNum - 1;
    while (idx >= 0) {
        LocalVar *var = &cu->localVars[idx];

        // 只在当前作用域中查找同名变量
        // 如果遇到了父作用域就退出，减少没有必要的遍历
        if (var->scopeDepth < cu->scopeDepth) {
            break;
        }

        // 重复
        if (var->length == length && memcmp(var->name, name, length) == 0) {
            char id[MAX_ID_LEN] = {'\0'};
            memcpy(id, name, length);
            COMPILE_ERROR(cu->curParser, "identifier \"%s\" redefinition!", id);
        }
        idx--;
    }
    // 检查过后声明该局部变量
    return addLocalVar(cu, name, length);
}

// 根据作用域声明变量
static int declareVariable(CompileUnit *cu, const char *name, uint32_t length) {
    // 若当前模块作用域就声明为模块变量
    if (cu->scopeDepth == -1) {
        int index = defineModuleVar(cu->curParser->vm, cu->curParser->curModule, name, length, VT_TO_VALUE(VT_NULL));
        if (index == -1) {
            // 重复定义就报错
            char id[MAX_ID_LEN] = {'\0'};
            memcpy(id, name, length);
            COMPILE_ERROR(cu->curParser, "identifier\"%s\" redefinition!", id);
        }
        return index;
    }
    // 否则是局部作用域，声明局部变量
    return declareLocalVar(cu, name, length);
}


// 声明模块变量，与defineModuleVar的区别是不做重定义检查，默认为声明
static int declareModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value) {
    ValueBufferAdd(vm, &objModule->moduleVarValue, value);
    return addSymbol(vm, &objModule->moduleVarName, name, length);
}

// 返回包含 cu->enclosingClassBK 的最近的 CompileUnit
static CompileUnit *getEnclosingClassBKUnit(CompileUnit *cu) {
    while (cu != NULL) {
        if (cu->enclosingClassBK != NULL) {
            return cu;
        }
        cu = cu->enclosingUnit;
    }
    return NULL;
}

// 返回包含cu最近的ClassBookKeep
static ClassBookKeep *getEnclosingClassBK(CompileUnit *cu) {
    CompileUnit *ncu = getEnclosingClassBKUnit(cu);
    if (ncu != NULL) {
        return ncu->enclosingClassBK;
    }
    return NULL;
}

// 为实参列表中的各个实参生成加载实参的指令
static void processArgList(CompileUnit *cu, Signature *sign) {
    // 由主调用方保证参数不空
    ASSERT(cu->curParser->curToken.type != TOKEN_RIGHT_PAREN &&
           cu->curParser->curToken.type != TOKEN_RIGHT_BRACKET, "empty argument list!");

    do {
        if (++sign->argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }
        expression(cu, BP_LOWEST); // 加载实参
    } while (matchToken(cu->curParser, TOKEN_COMMA));
}

// 声明形参列表中的各个形参
static void processParaList(CompileUnit *cu, Signature *sign) {
    ASSERT(cu->curParser->curToken.type != TOKEN_RIGHT_PAREN
           && cu->curParser->curToken.type != TOKEN_RIGHT_BRACKET,
           "empty argument-nt list!");
    do {
        if (++sign->argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }
        consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
        declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
    } while (matchToken(cu->curParser, TOKEN_COMMA));
}

// 尝试编译 setter
static bool trySetter(CompileUnit *cu, Signature *sign) {
    if (!matchToken(cu->curParser, TOKEN_ASSIGN)) {
        return false;
    }
    if (sign->type == SIGN_SUBSCRIPT) {
        sign->type = SIGN_SUBSCRIPT_SETTER;
    } else {
        sign->type = SIGN_SETTER;
    }

    // 读取等号右边的形参左边的（
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' after '='!");

    // 读取形参
    consumeCurToken(cu->curParser, TOKEN_ID, "expect ID!");
    // 声明形参
    declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
    // 读取等号右边的形参右边的)
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after argument list!");
    sign->argNum++;
    return true;
}

// 标识符的签名函数
static void idMethodSignature(CompileUnit *cu, Signature *sign) {
    sign->type = SIGN_GETTER; // 刚识别到id，默认为getter

    //  new 方法为构造函数
    if (sign->length == 3 && memcmp(sign->name, "new", 3) == 0) {
        // 构造函数后面不能接=，既不能成为setter
        if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
            COMPILE_ERROR(cu->curParser, "constructor shouldn't be setter!");
        }
        // 构造函数必须是标准的method，既 new(_, ....) new 后面必须接(
        if (!matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
            COMPILE_ERROR(cu->curParser, "constructor must bu method!");
        }

        sign->type = SIGN_CONSTRUCT;

        // 无参数就直接返回
        if (matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            return;
        }
    } else {
        // 若不是构造函数
        if (trySetter(cu, sign)) {
            // 若是setter 此时已经将 type改为了setter，直接返回
            return;
        }

        // 至此type应该为一般形式的SIGN_METHOD，形式为name（paralist)
        sign->type = SIGN_METHOD;

        // 直接匹配到 ），说明形参为空
        if (matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
            return;
        }
    }

    // 下面是处理形参
    processParaList(cu, sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter list!");
}

// 为单运算符方法创建签名
static void unaryMethodSignature(CompileUnit *cu UNUSED, Signature *sign UNUSED) {
    // 名称部分在调用前已经完成，只修改类型
    sign->type = SIGN_GETTER;
}


// 为既做单运算符又做中缀运算符的符号方法创建签名
static void mixMethodSignature(CompileUnit *cu, Signature *sign) {
    // 假设是单运算符方法，因此默认是getter
    sign->type = SIGN_GETTER;
    // 若后面有（，说明其为中缀运算符，那就置其类型为SIGN_METHOD
    if (matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
        sign->type = SIGN_METHOD;
        sign->argNum = 1;
        consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
        declareVariable(cu, cu->curParser->preToken.start, cu->curParser->preToken.length);
        consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter!");
    }
}

// 查找局部变量
static int findLocal(CompileUnit *cu, const char *name, uint32_t length) {
    // 内部作用域变量会覆盖外层，故从后往前，由最内存逐渐往外层找
    int index = cu->localVarNum - 1;
    while (index >= 0) {
        if (cu->localVars[index].length == length && memcmp(cu->localVars[index].name, name, length) == 0) {
            return index;
        }
        index--;
    }
    return -1;
}


// 查找name指代的 upvalue 后添加到 cu->upvalues , 返回其索引，否则返回-1
static int findUpvalue(CompileUnit *cu, const char *name, uint32_t length) {
    if (cu->enclosingUnit == NULL) { // 如果已经到了最外层仍未找到，返回-1
        return -1;
    }
    // 进入了方法的cu并且查找的不是静态域，即不是方法的Upvalue，那就没有必要再往上找了
    if (!strchr(name, ' ') && cu->enclosingUnit->enclosingClassBK != NULL) {
        return -1;
    }

    // 查看name是否为直接外层的局部变量
    int directOuterLocalIndex = findLocal(cu->enclosingUnit, name, length);

    // 若是，将该局部变量置为upvalue
    if (directOuterLocalIndex != -1) {
        cu->enclosingUnit->localVars[directOuterLocalIndex].isUpvalue = true;
        return addUpvalue(cu, true, (uint32_t) directOuterLocalIndex);
    }
    // 向外层递归查找 ,此时 cu 和 cu->enclosingUnit 是没有的，递归调用 findUpvalue 回去 cu->enclosingUnit->enclosingUnit 中查找
    int directOuterUpvalueIndex = findUpvalue(cu->enclosingUnit, name, length);
    if (directOuterUpvalueIndex != -1) {
        return addUpvalue(cu, false, (uint32_t) directOuterUpvalueIndex);
    }
    // 执行到此处说明没有该upvalue对应的局部变量 返回-1
    return -1;
}

// 从局部变量和upvalue中查找符号name
static Variable getVarFromLocalOrUpvalue(CompileUnit *cu, const char *name, uint32_t length) {
    Variable var;
    // 默认为无效作用域类型，查到后会被更正
    var.scopeType = VAR_SCOPE_INVALID;

    // 先从局部变量找
    var.index = findLocal(cu, name, length);
    if (var.index != -1) {
        var.scopeType = VAR_SCOPE_LOCAL;
        return var;
    }

    // 再从 upvalue中找
    var.index = findUpvalue(cu, name, length);
    if (var.index != -1) {
        var.scopeType = VAR_SCOPE_UPVALUE;
    }
    return var;
}


// 编译程序
static void compileProgram(CompileUnit *cu) {
    ;
}

// 编译变量定义
static void compileVarDefinition(CompileUnit *cu, bool isStatic) {
    consumeCurToken(cu->curParser, TOKEN_ID, "missing variable name!");
    Token name = cu->curParser->preToken;
    // 只支持定义单个变量
    if (cu->curParser->curToken.type == TOKEN_COMMA) {
        COMPILE_ERROR(cu->curParser, "'var' only support declaring a variable");
    }

    // 1. 先判断是否是类中的域定义，确保cu是模块cu
    if (cu->enclosingUnit == NULL && cu->enclosingClassBK != NULL) {

        if (isStatic) {
            // 静态域
            char *staticFieldId = ALLOCATE_ARRAY(cu->curParser->vm, char, MAX_ID_LEN);
            memset(staticFieldId, 0, MAX_ID_LEN);
            uint32_t staticFieldIdLen;
            char *clsName = cu->enclosingClassBK->name->value.start;
            uint32_t clsLen = cu->enclosingClassBK->name->value.length;

            // 使用前缀 " 'Cls ' + 类名 + 变量名" 作为静态域在模块编译单元中的局部变量
            memmove(staticFieldId, "Cls", 3);
            memmove(staticFieldId + 3, clsName, clsLen);
            memmove(staticFieldId + 3 + clsLen, " ", 1);
            const char *tkName = name.start;
            uint32_t tkLen = name.length;
            memmove(staticFieldId + 4 + clsLen, tkName, tkLen);
            staticFieldIdLen = strlen(staticFieldId);

            if (findLocal(cu, staticFieldId, staticFieldIdLen) == -1) {
                int index = declareLocalVar(cu, staticFieldId, staticFieldIdLen);
                writeOpCode(cu, OPCODE_PUSH_NULL);
                ASSERT(cu->scopeDepth == 0, "should in class scope!");
                defineVariable(cu, index);

                // 静态域可初始化
                Variable var = findVariable(cu, staticFieldId, staticFieldIdLen);
                if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    expression(cu, BP_LOWEST);
                    emitStoreVariable(cu, var);
                }

            } else {
                COMPILE_ERROR(cu->curParser, "static field '%s' redefinition!", strchr(staticFieldId, ' ') + 1);
            }

        } else {
            // 定义实例域
            ClassBookKeep *classBK = getEnclosingClassBK(cu);

            int fieldIndex = getIndexFromSymbolTable(&classBK->fields, name.start, name.length);
            if (fieldIndex == -1) {
                fieldIndex = addSymbol(cu->curParser->vm, &classBK->fields, name.start, name.length);
            } else {
                if (fieldIndex > MAX_FIELD_NUM) {
                    COMPILE_ERROR(cu->curParser, "the max number of instance field is %d!", MAX_FIELD_NUM);
                } else {
                    char id[MAX_ID_LEN] = {'\0'};
                    memcpy(id, name.start, name.length);
                    COMPILE_ERROR(cu->curParser, "instance field '%s' redefinition!", id);
                }
                if (matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    COMPILE_ERROR(cu->curParser, "instance field isn't allowed initialization!");
                }
            }
        }
        return;
    }

    // 2. 若不是类中的域定义，就按照一般的变量定义
    if(matchToken(cu->curParser, TOKEN_ASSIGN)){
        // 若在定义是赋值就解析表达式，结果会留到栈顶
        expression(cu, BP_LOWEST);
    }else{
        // 否则就初始化为NULL,即在栈顶压入NULL
        // 也是为了与上面显式初始化保持相同栈结构
        writeOpCode(cu, OPCODE_PUSH_NULL);
    }

    uint32_t index = declareVariable(cu, name.start, name.length);
    defineVariable(cu, index);
}



// 编译 if 语句
static void compileIfStatement(CompileUnit* cu){
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "missing '(' after if!");
    expression(cu, BP_LOWEST); // 生成计算if条件表示式的指令步骤
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "missing ')' before '{' in if!");

    // 若条件为假，if跳转到false 分支的起始地址，现为该地址设置占位符
    uint32_t falseBranchStart = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 编译 then 分支
    // 代码块前后的{ 和 } 由 compileStatement 负责读取
    compileStatement(cu);

    // 如果有else分支
    if(matchToken(cu->curParser, TOKEN_ELSE)){
        // 添加跳过else分支的跳转指令
        uint32_t falseBranchEnd = emitInstrWithPlaceholder(cu, OPCODE_JUMP);

        // 你入else分支编译前，先会填falseBranchStart
        patchPlaceholder(cu, falseBranchStart);

        // 编译else分支
        compileStatement(cu);

        // 此时知道了false块的结束地址，回填falseBranchEnd
        patchPlaceholder(cu, falseBranchEnd);
    }else{
        // 若不包括else块
        // 此时 falseBranchStart 就是条件为假时，需跳过整个 true分支的目标地址
        patchPlaceholder(cu, falseBranchStart);
    }
}

// 编译语句（即程序中与声明，定义无关的、表示"动作"的代码）
static void compileStatement(CompileUnit* cu){
    if(matchToken(cu->curParser, TOKEN_IF)){
        compileIfStatement(cu);
    }else if(matchToken(cu->curParser, TOKEN_WHILE)){
        compileWhileStatement(cu);
    }else if(matchToken(cu->curParser, TOKEN_FOR)){
        compileForStatement(cu);
    }
    else if(matchToken(cu->curParser, TOKEN_RETURN)){
        compileReturn(cu);
    }else if(matchToken(cu->curParser, TOKEN_BREAK)){
        compileBreak(cu);
    }else if(matchToken(cu->curParser, TOKEN_CONTINUE)) {
        compileContinue(cu);
    }else if(matchToken(cu->curParser, TOKEN_LEFT_BRACE)){
        // 代码块有单独的作用域
        enterScope(cu);
        compileBlock(cu);
        leaveScope(cu);
    }else{
        // 若不是以上的语法结构则是单一表示式
        expression(cu, BP_LOWEST);

        // 表达式的结果不重要，弹出栈定结果
        writeOpCode(cu, OPCODE_POP);
    }
}


// 开始循环，进入循环体的相关设置
static void enterLoopSetting(CompileUnit* cu, Loop* loop){
    // cu->fn->instrStream.count 是下一条指令的地址，所以-1
    loop->condStartIndex = cu->fn->instrStream.count - 1;

    loop->scopeDepth = cu->scopeDepth;

    // 在当前循环层中嵌套新的循环层，当前层称为内嵌层的外层
    loop->enclosingLoop = cu->curLoop;

    // 使 cu->curLoop 指向新的内层
    cu->curLoop = loop;
}

// 编译循环体
static void compileLoopBody(CompileUnit* cu){
    // 使循环体起始地址指向下一条指令地址
    cu->curLoop->bodyStartIndex = cu->fn->instrStream.count;
    compileStatement(cu);
}


//获得ip所指向的操作码的操作数占用的字节数
uint32_t getBytesOfOperands(Byte* instrStream,
                            Value* constants, int ip) {

    switch ((OpCode)instrStream[ip]) {
        case OPCODE_CONSTRUCT:
        case OPCODE_RETURN:
        case OPCODE_END:
        case OPCODE_CLOSE_UPVALUE:
        case OPCODE_PUSH_NULL:
        case OPCODE_PUSH_FALSE:
        case OPCODE_PUSH_TRUE:
        case OPCODE_POP:
            return 0;

        case OPCODE_CREATE_CLASS:
        case OPCODE_LOAD_THIS_FIELD:
        case OPCODE_STORE_THIS_FIELD:
        case OPCODE_LOAD_FIELD:
        case OPCODE_STORE_FIELD:
        case OPCODE_LOAD_LOCAL_VAR:
        case OPCODE_STORE_LOCAL_VAR:
        case OPCODE_LOAD_UPVALUE:
        case OPCODE_STORE_UPVALUE:
            return 1;

        case OPCODE_CALL0:
        case OPCODE_CALL1:
        case OPCODE_CALL2:
        case OPCODE_CALL3:
        case OPCODE_CALL4:
        case OPCODE_CALL5:
        case OPCODE_CALL6:
        case OPCODE_CALL7:
        case OPCODE_CALL8:
        case OPCODE_CALL9:
        case OPCODE_CALL10:
        case OPCODE_CALL11:
        case OPCODE_CALL12:
        case OPCODE_CALL13:
        case OPCODE_CALL14:
        case OPCODE_CALL15:
        case OPCODE_CALL16:
        case OPCODE_LOAD_CONSTANT:
        case OPCODE_LOAD_MODULE_VAR:
        case OPCODE_STORE_MODULE_VAR:
        case OPCODE_LOOP:
        case OPCODE_JUMP:
        case OPCODE_JUMP_IF_FALSE:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_INSTANCE_METHOD:
        case OPCODE_STATIC_METHOD:
            return 2;

        case OPCODE_SUPER0:
        case OPCODE_SUPER1:
        case OPCODE_SUPER2:
        case OPCODE_SUPER3:
        case OPCODE_SUPER4:
        case OPCODE_SUPER5:
        case OPCODE_SUPER6:
        case OPCODE_SUPER7:
        case OPCODE_SUPER8:
        case OPCODE_SUPER9:
        case OPCODE_SUPER10:
        case OPCODE_SUPER11:
        case OPCODE_SUPER12:
        case OPCODE_SUPER13:
        case OPCODE_SUPER14:
        case OPCODE_SUPER15:
        case OPCODE_SUPER16:
            //OPCODE_SUPERx的操作数是分别由writeOpCodeShortOperand
            //和writeShortOperand写入的,共1个操作码和4个字节的操作数
            return 4;

        case OPCODE_CREATE_CLOSURE: {
            //获得操作码OPCODE_CLOSURE 操作数,2字节.
            //该操作数是待创建闭包的函数在常量表中的索引
            uint32_t fnIdx = (instrStream[ip + 1] << 8) | instrStream[ip + 2];

            //左边第1个2是指fnIdx在指令流中占用的空间
            //每个upvalue有一对儿参数
            return 2 + (VALUE_TO_OBJFN(constants[fnIdx]))->upvalueNum * 2;
        }

        default:
            NOT_REACHED();
    }
}

// 离开循环体时的相关设置
static void leaveLoopPath(CompileUnit* cu){
    // 获取往回跳转的偏移量，偏移量都为正数
    int loopBackOffset = cu->fn->instrStream.count - cu->curLoop->condStartIndex + 2;

    // 生成向回跳转的 CODE_LOOP 指令，既IP -= loopBackOffset
    writeOpCodeShortOperand(cu, OPCODE_LOOP, loopBackOffset);

    // 回填循环体的结束地址
    patchPlaceholder(cu, cu->curLoop->exitIndex);

    // 下面在循环体中回填break的占位符OPCODE_END
    // 循环体开始地址
    uint32_t idx = cu->curLoop->bodyStartIndex;
    // 循环体结束地址
    uint32_t loopEndIndex = cu->fn->instrStream.count;
    while(idx < loopEndIndex){
        // 回填循环体内所有可能的break语句
        if(OPCODE_END == cu->fn->instrStream.datas[idx]){
            cu->fn->instrStream.datas[idx] = OPCODE_JUMP;
            // 回填OPCODE_JUMP 的操作数，即跳转偏移量

            // id+1 是操作数的高字节，patchPlaceholder 中会去处理 idx+1 和 idx+2
            patchPlaceholder(cu, idx+1);

            // 使 idx 指向指令流中下一操作码
            idx += 3;
        }else{
            // 为提高遍历速度，遇到不是OPCODE_END的指令
            // 一次跳过该指令及其操作数
            idx += 1 + getBytesOfOperands(cu->fn->instrStream.datas, cu->fn->constants.datas, idx);
        }
    }

    // 退出当前循环体， 既恢复 cu->curLoop 为当前循环层的外层循环
    cu->curLoop = cu->curLoop->enclosingLoop;
}

// 编译 while 循环，如 while(a < a) { 循环体 }
static void compileWhileStatement(CompileUnit* cu ){
    Loop loop;

    // 设置循环体其实地址等等
    enterLoopSetting(cu, &loop);
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' before condition!");

    // 生成计算条件表达式的指令步骤，结果在栈顶
    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after condition!");

    // 先把条件失败时跳转的目标地址占位
    loop.exitIndex =  emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);
    compileLoopBody(cu);

    // 设置循环体结束等等
    leaveLoopPath(cu);

}

// 丢掉作用于scopeDepth之内的局部变量
static uint32_t discardLocalVar(CompileUnit* cu, int scopeDepth){
    ASSERT(cu->scopeDepth > -1, "upmost scope can't exit!");
    int localIdx = cu->localVarNum - 1;

    // 变量作用于大于 scopeDepth 的为其内嵌作用域中的变量
    // 跳出 scopeDepth 时内层也没用了，要回收其局部变量
    while(localIdx >= 0 && cu->localVars[localIdx].scopeDepth >= scopeDepth){
        if(cu->localVars[localIdx].isUpvalue){
            // 如果此局部变量是其内存的 upvalue 就将其关闭
            writeByte(cu, OPCODE_CLOSE_UPVALUE);
        }else{
            // 否则就弹出该变量回收空间
            writeByte(cu, OPCODE_POP);
        }
        localIdx--;
    }
    // 返回丢掉的局部变量个数
    return cu->localVarNum - 1 - localIdx;
}

// 编译return
inline static void compileReturn(CompileUnit* cu){
   if(PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_PAREN) { // 空返回值
       // 空 return， NULL作为返回值
       writeOpCode(cu, OPCODE_PUSH_NULL);
   }else{ // 有返回值
       expression(cu, BP_LOWEST);
   }
   writeOpCode(cu, OPCODE_RETURN);  // 将上面栈顶的值返回
}

// 编译 break
inline static void compileBreak(CompileUnit* cu){
    if(cu->curLoop == NULL){
        COMPILE_ERROR(cu->curParser, "break should be used inside a loop!");
    }

    // 在退出循环体之前要丢掉循环体内部的局部变量
    discardLocalVar(cu, cu->curLoop->scopeDepth + 1);

    // 由于用 OPCODE_END 表示 break 占位，此时无需记录占位符的返回地址
    emitInstrWithPlaceholder(cu, OPCODE_END);
}

// 编译 continue
inline static void compileContinue(CompileUnit* cu){
    if(cu->curLoop == NULL){
        COMPILE_ERROR(cu->curParser, "continue should be used inside a loop!");
    }

    // 回收本作用域中局部变量在栈中的空间，+1 是指循环体（包括循环条件）的作用域
    // 不能在 cu->localVars 数组中去掉
    // 否则在continue 语句后面引用了前面的变量则提示找不到
    discardLocalVar(cu, cu->curLoop->scopeDepth + 1);

    int loopbackOffset = cu->fn->instrStream.count - cu->curLoop->condStartIndex + 2;

    // 生成向回跳转的 CODE_LOOP 指令， 即使 ip -= loopBackOffset
    writeOpCodeShortOperand(cu, OPCODE_LOOP, loopbackOffset);
}

// 编译代码块
static void compileBlock(CompileUnit *cu) {
    // 进入本函数已读入了{
    while (!matchToken(cu->curParser, TOKEN_RIGHT_BRACE)) {
        if (PEEK_TOKEN(cu->curParser) == TOKEN_EOF) {
            COMPILE_ERROR(cu->curParser, "expect ')' at the end of block!");
        }
        compileProgram(cu);
    }
}

// 编译函数或方法体
static void compileBody(CompileUnit *cu, bool isConstruct) {
    // 进入本函数前已经读入了{
    compileBlock(cu);
    if (isConstruct) {
        // 若是构造函数就加载 “this对象“作为下面OPCODE_RETURN的返回值
        writeOpCodeByteOperand(cu, OPCODE_LOAD_LOCAL_VAR, 0);
    } else {
        // 否则加载NULL占位
        writeOpCode(cu, OPCODE_PUSH_NULL);
    }

    // 返回编译结果，若是构造函数就返回this ，否则返回null
    writeOpCode(cu, OPCODE_RETURN);
}

// 结束 cu 的编译工作，在其外层编译单元中为其创建闭包
#if DEBUG
static ObjFn* endCompileUnit(CompileUnit* cu, const char* debugName, uint32_t debugNameLen){
    bindDebugFnName(cu->curParser->vm, cu->fn->debug, debugName, debugNameLen);
#else

static ObjFn *endCompileUnit(CompileUnit *cu) {
#endif
    // 标识单元编译结束
    writeOpCode(cu, OPCODE_END);
    if (cu->enclosingUnit != NULL) {
        // 把当前编译的 objFn 作为常量添加到父编译单元的常量表
        uint32_t index = addConstant(cu->enclosingUnit, OBJ_TO_VALUE(cu->fn));
        // 内层函数以闭包形式在外层函数中存在
        // 在外层函数中的指令流中添加“为当前内层函数创建闭包的指令”
        writeOpCodeShortOperand(cu->enclosingUnit, OPCODE_CREATE_CLOSURE, index);

        // 为vm在创建闭包时判断引用的局部变量还是 upvalue
        // 下面为每个upvalue生成参数
        index = 0;
        while (index < cu->fn->upvalueNum) {
            writeByte(cu->enclosingUnit, cu->upvalues[index].isEnclosingLocalVar ? 1 : 0);
            writeByte(cu->enclosingUnit, cu->upvalues[index].index);
            index++;
        }
    }

    //  下调本编译单元，使当前编译单元指向外层编译单元
    cu->curParser->curCompileUnit = cu->enclosingUnit;
    return cu->fn;
}


// 编译模块
ObjFn *compileModule(VM *vm, ObjModule *objModule, const char *moduleCode) {
    // 各源码模块文件需要单独的parser
    Parser parser;
    parser.parent = vm->curParser;
    vm->curParser = &parser;

    if (objModule->name == NULL) {
        // 核心模块是core.script.inc
        initParser(vm, &parser, "core.script.inc", moduleCode, objModule);
    } else {
        initParser(vm, &parser, (const char *) objModule->name->value.start, moduleCode, objModule);
    }

    CompileUnit moduleCU;
    initCompileUnit(&parser, &moduleCU, NULL, false);

    // 记录现在模块变量的数量，后面检查预定义模块变量时可减少遍历
    uint32_t moduleVarNumBefor = objModule->moduleVarValue.count;

    // 初始的parser->curToken.type 为TOKEN_UNKNOWN,下面使其指向第一个合法的token
    getNextToken(&parser);

    // 此时compileProgram为桩函数
    // 不过目前上面是死循环，本句无法执行
    printf("There is something to do....\n");
    exit(0);
}


// 符号绑定规则
SymbolBindRule Rules[] = {
        /* TOKEN_INVALID */                 UNUSED_RULE, // 符合 SymbolBindRule 的参数结构
        /* TOKEN_NUM */                     PREFIX_SYMBOL(literal),
        /* TOKEN_STRING */                  PREFIX_SYMBOL(literal),
        /* TOKEN_ID*/                       {NULL, BP_NONE, id, NULL, idMethodSignature},
        /* TOKEN_INTERPOLATION */           PREFIX_SYMBOL(stringInterpolation),
        /* TOKEN_VAR */                     UNUSED_RULE,
        /* TOKEN_FUN */                     UNUSED_RULE,
        /* TOKEN_IF */                      UNUSED_RULE,
        /* TOKEN_ELSE */                    UNUSED_RULE,
        /* TOKEN_TRUE */                    PREFIX_SYMBOL(boolean),
        /* TOKEN_FALSE */                   PREFIX_SYMBOL(boolean),
        /* TOKEN_WHILE */                   UNUSED_RULE,
        /* TOKEN_FOR */                     UNUSED_RULE,
        /* TOKEN_BREAK */                   UNUSED_RULE,
        /* TOKEN_CONTINUE */                UNUSED_RULE,
        /* TOKEN_RETURN */                  UNUSED_RULE,
        /* TOKEN_NULL */                    PREFIX_SYMBOL(null),
        /* TOKEN_CLASS */                   UNUSED_RULE,
        /* TOKEN_THIS */                    PREFIX_SYMBOL(this),
        /* TOKEN_STATIC */                  UNUSED_RULE,
        /* TOKEN_IS */                      INFIX_OPERATOR("is", BP_IS),
        /* TOKEN_SUPER */                   PREFIX_SYMBOL(super),
        /* TOKEN_IMPORT */                  UNUSED_RULE,
        /* TOKEN_COMMA */                   UNUSED_RULE,
        /* TOKEN_COMMA */                   UNUSED_RULE,
        /* TOKEN_LEFT_PAREN */              PREFIX_SYMBOL(parentheses),
        /* TOKEN_RIGHT_PAREN */             UNUSED_RULE,
        /* TOKEN_LEFT_BRACKET */            {NULL, BP_CALL, listLiteral, subscript, subscriptMethodSignature},
        /* TOKEN_RIGHT_BRACKET */           UNUSED_RULE,
        /* TOKEN_LEFT_BRACE */              PREFIX_SYMBOL(mapLiteral),
        /* TOKEN_RIGHT_BRACE */             UNUSED_RULE,
        /* TOKEN_DOT */                     INFIX_SYMBOL(BP_CALL, callEntry),
        /* TOKEN_DOT_DOT */                 INFIX_OPERATOR("..", BP_RANGE),
        /* TOKEN_ADD */                     INFIX_OPERATOR("+", BP_TERM),
        /* TOKEN_SUB */                     MIX_OPERATOR("-"),
        /* TOKEN_MUL */                     INFIX_OPERATOR("*", BP_FACTOR),
        /* TOKEN_DIV */                     INFIX_OPERATOR("/", BP_FACTOR),
        /* TOKEN_MOD */                     INFIX_OPERATOR("%", BP_FACTOR),
        /* TOKEN_ASSIGN */                  UNUSED_RULE,
        /* TOKEN_BIT_AND */                 INFIX_OPERATOR("&", BP_BIT_AND),
        /* TOKEN_BIT_OR */                  INFIX_OPERATOR("|", BP_BIT_OR),
        /* TOKEN_BIT_NOT */                 PREFIX_OPERATION("~"),
        /* TOKEN_BIT_SHIFT_RIGHT */         INFIX_OPERATOR(">>", BP_BIT_SHIFT),
        /* TOKEN_BIT_SHIFT_LEFT */          INFIX_OPERATOR("<<", BP_BIT_SHIFT),
        /* TOKEN_LOGIC_AND */               INFIX_SYMBOL(BP_LOGIC_AND, logicAnd),
        /* TOKEN_LOGIC_OR */                INFIX_SYMBOL(BP_LOGIC_OR, logicOr),
        /* TOKEN_LOGIC_NOT */               PREFIX_OPERATION("!"),
        /* TOKEN_EQUAL */                   INFIX_OPERATOR("==", BP_EQUAL),
        /* TOKEN_NOT_EQUAL */               INFIX_OPERATOR("!=", BP_EQUAL),
        /* TOKEN_GREATE */                  INFIX_OPERATOR(">", BP_CMP),
        /* TOKEN_GREATE_EQUAL */            INFIX_OPERATOR(">=", BP_CMP),
        /* TOKEN_LESS */                    INFIX_OPERATOR("<", BP_CMP),
        /* TOKEN_LESS_EQUAL */              INFIX_OPERATOR("<=", BP_CMP),
        /* TOKEN_QUESTION */                INFIX_SYMBOL(BP_ASSIGN, condition),
        /* TOKEN_EOF */                     UNUSED_RULE,
};

// 数字和字符串.nud() 编译字面量
static void literal(CompileUnit *cu, bool canAssign UNUSED) {
    // literal 是常量（数字和字符串）的nud方法，用来返回字面值
    emitLoadConstant(cu, cu->curParser->preToken.value);
}


// 内嵌表达式的.nud()
static void stringInterpolation(CompileUnit *cu, bool canAssign UNUSED) {
    // a % (a+c) d %(e) f
    // 会按照以下形式进行编译
    // ["a ", a+c, " d ", e, "f "].join()
    // 其中 a 和 d 是TOKEN_INTERPOLATION, a c e 都是 TOKEN_ID， f 是 TOKEN_STRING

    // 创建一个 list实例，拆分字符串， 将拆分出的各部分作为元素添加到list
    emitLoadModuleVar(cu, "List");
    emitCall(cu, 0, "new()", 5);

// 每次处理字符串中的一个内嵌表达式，包括两部分， 以 a %(a+c) 为例 :
//      1. 加载 TOKEN_INTERPOLATION 对应的字符串，如 a，添加到list
//      2. 解析内嵌表达式，如 a+c , 将结果添加到list
    do {
        // 1.处理 TOKEN_INTERPOLATION 中的字符串，如果 a %(a+c) 中的a
        literal(cu, false);
        // 将字符串添加到list
        emitCall(cu, 1, "addCore_(_)", 11); //  以_结尾的方法名是内部使用

        // 2. 解析内嵌表达式，如果 a %(a+c) 中的b+c
        expression(cu, BP_LOWEST);
        // 将结果添加到list
        emitCall(cu, 1, "addCore_(_)", 11);
    } while (matchToken(cu->curParser, TOKEN_INTERPOLATION));
    // 处理下一个内嵌表达式,如 a %(a+c) d %(e) f 中的 d %(e)
    consumeCurToken(cu->curParser, TOKEN_STRING, "expect string at the end of interpolation!");

    // 加载最后的字符串
    literal(cu, false);

    // 将字符串添加到list
    emitCall(cu, 1, "addCore_(_)", 11);

    // 最后将list中的元素 join() 为一个字符串
    emitCall(cu, 0, "join()", 6);

}

// 编译 bool
static void boolean(CompileUnit *cu, bool canAssign UNUSED) {
    // true 和 false 的 nud 方法
    OpCode opCode = cu->curParser->preToken.type == TOKEN_TRUE ? OPCODE_PUSH_TRUE : OPCODE_PUSH_FALSE;
    writeOpCode(cu, opCode);
}

// 生成 OPCODE_PUSH_NULL 指令
static void null(CompileUnit *cu, bool canAssign UNUSED) {
    writeOpCode(cu, OPCODE_PUSH_NULL);
}

// "this".nud()
static void this(CompileUnit *cu, bool canAssign UNUSED) {
    if (getEnclosingClassBK(cu) == NULL) {
        COMPILE_ERROR(cu->curParser, "this must be inside a class method!");
    }
    emitLoadThis(cu);
}

// "super".nud()
static void super(CompileUnit *cu, bool canAssign) {
    ClassBookKeep *enclosingClassBK = getEnclosingClassBK(cu);
    if (enclosingClassBK == NULL) {
        COMPILE_ERROR(cu->curParser, "can't invoke super outside a class method!");
    }

    // 此处加载this， 是保证参数 args[0] 始终是this对象，尽管对于基类调用无用
    emitLoadThis(cu);

    // 判断形式 super.methodname()
    if (matchToken(cu->curParser, TOKEN_DOT)) {
        consumeCurToken(cu->curParser, TOKEN_ID, "expect name after '.'!");
        emitMethodCall(cu, cu->curParser->preToken.start, cu->curParser->preToken.length, OPCODE_SUPER0, canAssign);
    } else {
        // super(): 调用基类中与关键字super所在子类方法同名的方法
        emitGetterMethodCall(cu, enclosingClassBK->signature, OPCODE_SUPER0);
    }
}

// 编译圆括号
static void parentheses(CompileUnit *cu, bool canAssign UNUSED) {
    // 本函数是'('.nud, 假设 curToken是(
    expression(cu, BP_LOWEST);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after expression!");
}

// '[].nud 处理用字面量形式定义的list列表
static void listLiteral(CompileUnit *cu, bool canAssign UNUSED) {
    // 进入本函数后，curToken 是[ 右边的符号

    // 先创建list对象
    emitLoadModuleVar(cu, "List");
    emitCall(cu, 0, "new()", 5);

    do {
        // 支持字面量形式定义的空列表
        if (PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_BRACKET) {
            break;
        }
        expression(cu, BP_LOWEST);
        emitCall(cu, 1, "addCore_(_)", 11);
    } while (matchToken(cu->curParser, TOKEN_COMMA));

    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACKET, "expect ')' after list element!");
}

// '['.led() 用于索引 list 元素，如list[index]
static void subscript(CompileUnit *cu, bool canAssign) {
    // 确保[] 之间不空
    if (matchToken(cu->curParser, TOKEN_RIGHT_BRACKET)) {
        COMPILE_ERROR(cu->curParser, "need argument in the '[]'!");
    }

    // 默认是[_] 即 subscript getter
    Signature sign = {SIGN_SUBSCRIPT, "", 0, 0};

    // 读取参数并把参数加载到栈中，统计参数个数
    processArgList(cu, &sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACKET, "expect ']' after argument list!");

    // 若是[_] = (_) 既subscript setter
    if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
        sign.type = SIGN_SUBSCRIPT_SETTER;

        // = 右边的值也算一个参数，签名是 [args[1]] = (args[2])
        if (++sign.argNum > MAX_ARG_NUM) {
            COMPILE_ERROR(cu->curParser, "the max number of argument is %d!", MAX_ARG_NUM);
        }

        // 获取 = 右边的表达式
        expression(cu, BP_LOWEST);
    }
    emitCallBySignature(cu, &sign, OPCODE_CALL0);
}

// 以下为操作符[ 编译签名
static void subscriptMethodSignature(CompileUnit *cu, Signature *sign) {
    sign->type = SIGN_SUBSCRIPT;
    sign->length = 0;
    processParaList(cu, sign);
    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACKET, "expect  ']' after index list!");
    trySetter(cu, sign); // 判断 ] 后面是否接=为setter
}

// '.'.led() 编译方法调用，所有调用的入口
static void callEntry(CompileUnit *cu, bool canAssign) {
    // 本函数是'.'.led(), curToken 是 TOKEN_ID
    consumeCurToken(cu->curParser, TOKEN_ID, "expect method name after '.' !");

    // 生成方法调用指令
    emitMethodCall(cu, cu->curParser->curToken.start, cu->curParser->preToken.length, OPCODE_CALL0, canAssign);
}

// map 对象字面量
static void mapLiteral(CompileUnit *cu, bool canAssign UNUSED) {
    // 本函数是 '{'.nud(), curToken 是 key

    // Map.new() 新建map， 为存储字面量中的 key->value 作准备

    // 先加载类，用于调用方法时从该类的methods中定位方法
    emitLoadModuleVar(cu, "Map");
    // 再加载调用的方法，该方法将在上面加载的类中获取
    emitCall(cu, 0, "new()", 5);

    do {
        // 可以创建空map
        if (PEEK_TOKEN(cu->curParser) == TOKEN_RIGHT_BRACE) {
            break;
        }

        // 读取key
        expression(cu, BP_UNARY);

        // 读入key后面的冒号
        consumeCurToken(cu->curParser, TOKEN_COLON, "expect ':' after key!");

        // 读取value
        expression(cu, BP_LOWEST);

        // 将entry 添加map中
        emitCall(cu, 2, "addCore_(_,_)", 13);

        // 读取 value
        expression(cu, BP_LOWEST);

        // 将 entry 添加到map中
        emitCall(cu, 2, "addCore_(_,_)", 13);
    } while (matchToken(cu->curParser, TOKEN_COMMA));

    consumeCurToken(cu->curParser, TOKEN_RIGHT_BRACE, "map literal should end with \')\'!");
}

// 用占位符作为参数设置指令
static uint32_t emitInstrWithPlaceholder(CompileUnit *cu, OpCode opCode) {
    writeOpCode(cu, opCode);
    writeByte(cu, 0xff); // 先写入高位的 0xff

    // 再写入地位的0xff后，减1返回高位地址，此地址将来用于回填
    return writeByte(cu, 0xff) - 1;
}

// 用挑战到当前字节码结束地址的偏移量去替换占位符参数 0xffff
// absIndex 是指令流中绝对索引
static void patchPlaceholder(CompileUnit *cu, uint32_t absIndex) {
    // 计算回填地址（索引）
    uint32_t offset = cu->fn->instrStream.count - absIndex - 2;

    // 先回填地址高8位
    cu->fn->instrStream.datas[absIndex] = (offset >> 8) & 0xff;

    // 再回填地址低8位
    cu->fn->instrStream.datas[absIndex + 1] = offset & 0xff;
}

// '||'.led()
static void logicOr(CompileUnit *cu, bool canAssign UNUSED) {
    // 此时栈顶是条件表达式的结果， 即｜｜的左边操作数

    // 操作码 OPCODE_OR 会到栈顶获取条件
    uint32_t placeholderIndex = emitInstrWithPlaceholder(cu, OPCODE_OR);

    // 生成计算右操作数的指令
    expression(cu, BP_LOGIC_OR);

    // 用右表达式的十几结束地址回填OPCODE_OR操作码的占位符
    patchPlaceholder(cu, placeholderIndex);
}

// '&&'.led()
static void logicAnd(CompileUnit *cu, bool canAssign UNUSED) {
    // 此时栈顶是表达式的结果，既 && 的左操作数

    // 操作码 OPCODE_AND 会到栈顶获取条件
    uint32_t placeholderIndex = emitInstrWithPlaceholder(cu, OPCODE_AND);

    // 生成计算右操作数的指令
    expression(cu, BP_LOGIC_AND);

    // 用右表达式的实际结束地址回填OPCODE_AND 操作码的占位符
    patchPlaceholder(cu, placeholderIndex);
}

// "? : ".led()
static void condition(CompileUnit *cu, bool canAssign UNUSED) {
    // 若condition 为false ， if跳转到false分支的起始地址，为该地址设置占位符
    uint32_t falseBranchStart = emitInstrWithPlaceholder(cu, OPCODE_JUMP_IF_FALSE);

    // 编译 true 分支
    expression(cu, BP_LOWEST);

    consumeCurToken(cu->curParser, TOKEN_COLON, "expect ':' after true branch !");

    // 执行完true 分支后需要跳过false 分支
    uint32_t falseBranchEnd = emitInstrWithPlaceholder(cu, OPCODE_JUMP);

    // 编译true 分支已经结束，此时知道了true分支的结束地址
    // 编译false 分支之前需先回填 falseBranchStart
    patchPlaceholder(cu, falseBranchStart);

    // 编译 false 分支
    expression(cu, BP_LOWEST);

    // 知道了false分支的结束地址，回填falseBranchEnd
    patchPlaceholder(cu, falseBranchEnd);
}


// 小写字符开头便是局部变量
static bool isLocalName(const char *name) {
    return (name[0] >= 'a' && name[0] <= 'z');
}

// 标识符 .nud()： 变量名或方法名
static void id(CompileUnit *cu, bool canAssign) {
    // 备份变量名
    Token name = cu->curParser->preToken;
    ClassBookKeep *classBK = getEnclosingClassBK(cu);

    // 标识符可以是任意符号， 按照此顺序处理
    // 函数调用->局部变量和upvalue->实例域->静态域->类getter方法调用->模块变量

    // 处理函数调用
    if (cu->enclosingUnit == NULL && matchToken(cu->curParser, TOKEN_LEFT_PAREN)) {
        char id[MAX_ID_LEN] = {'\0'};

        // 函数名加上 "Fn " 前缀作为模块变量名称
        // 检查前面是否已有此函数的定义
        memmove(id, "Fn ", 3);
        memmove(id + 3, name.start, name.length);

        Variable var;
        var.scopeType = VAR_SCOPE_MODULE;
        var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, id, strlen(id));

        if (var.index == -1) {
            memmove(id, name.start, name.length);
            id[name.length] = '\0';
            COMPILE_ERROR(cu->curParser, "Undefined function:'%s' !", id);
        }

// 1. 把模块变量即函数闭包加载到栈中
        emitLoadVariable(cu, var);

        Signature sign;
        // 函数调用的形式和method类似
        // 只不过method有一个可选的块参数
        sign.type = SIGN_METHOD;

        // 把函数调用编译为 "闭包.call" 的形式，故name 为call
        sign.name = "call";
        sign.length = 4;
        sign.argNum = 0;

        // 若后面不是), 说明有参数列表
        if (!matchToken(cu->curParser, TOKEN_RIGHT_PAREN)) {
// 2. 压入实参
            processArgList(cu, &sign);
            consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after argument list!");
        }

// 3.生成调用指令以调用函数
        emitCallBySignature(cu, &sign, OPCODE_CALL0);
    } else { // 否则按照各种变量来处理
        // 按照局部变量和upvalue来处理
        Variable var = getVarFromLocalOrUpvalue(cu, name.start, name.length);
        if (var.index != -1) {
            emitLoadOrStoreVariable(cu, canAssign, var);
            return;
        }

        // 按照实例域来处理
        if (classBK != NULL) {
            int fieldIndex = getIndexFromSymbolTable(&classBK->fields, name.start, name.length);
            if (fieldIndex != -1) {
                bool isRead = true;
                if (canAssign && matchToken(cu->curParser, TOKEN_ASSIGN)) {
                    isRead = false;
                    expression(cu, BP_LOWEST);
                }

                // 如果当前正在编译类方法， 则直接在该实例对象中加载field
                if (cu->enclosingUnit != NULL) {
                    writeOpCodeByteOperand(cu, isRead ? OPCODE_LOAD_THIS_FIELD : OPCODE_STORE_THIS_FIELD, fieldIndex);
                } else {
                    emitLoadThis(cu);
                    writeOpCodeByteOperand(cu, isRead ? OPCODE_LOAD_FIELD : OPCODE_STORE_FIELD, fieldIndex);
                }
                return;
            }
        }
        // 按照静态域查找
        if (classBK != NULL) {
            char *staticFieldId = ALLOCATE_ARRAY(cu->curParser->vm, char, MAX_ID_LEN);
            memset(staticFieldId, 0, MAX_ID_LEN);
            uint32_t staticFieldIdLen;
            char *clsName = classBK->name->value.start;
            uint32_t clsLen = classBK->name->value.length;

            // 各类中静态域的名称以 "Cls 类名 静态域名" 来命名
            memmove(staticFieldId, "Cls", 3);
            memmove(staticFieldId + 3, clsName, clsLen);
            memmove(staticFieldId + 3 + clsLen, " ", 1);
            const char *tkName = name.start;
            uint32_t tkLen = name.length;
            memmove(staticFieldId + 4 + clsLen, tkName, tkLen);
            staticFieldIdLen = strlen(staticFieldId);
            var = getVarFromLocalOrUpvalue(cu, staticFieldId, staticFieldIdLen);
            DEALLOCATE_ARRAY(cu->curParser->vm, staticFieldId, MAX_ID_LEN);
            if (var.index != -1) {
                emitLoadOrStoreVariable(cu, canAssign, var);
                return;
            }
        }

        // 如果以上未找到同名变量， 有可能标识符是同类中的其他方法调用
        // 方法规定以小写字符开头
        if (classBK != NULL && isLocalName(name.start)) {
            emitLoadThis(cu); // 确保args[0] 是this对象，以便查找到方法
            // 因为类肯能尚未编译完成，未统计完所有方法,
            // 故此时无法判断方法是否为未定义，留待运行时检测
            emitMethodCall(cu, name.start, name.length, OPCODE_CALL0, canAssign);
            return;
        }

        // 按照模块变量处理
        var.scopeType = VAR_SCOPE_MODULE;
        var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, name.start, name.length);
        if (var.index == -1) {
            // 模块变量属于模块作用域，若当前引用处之前尚未定义该模块变量
            // 说不定在后面有其定义，因此暂时先声明它，待模块统计完之后再检查

            // 用关键字fun定义的函数是以前缀 Fn 后接函数名作为模块变量
            // 下面加上Fn 前缀按照函数名重新查找
            char fnName[MAX_SIGN_LEN + 4] = {'\0'};
            memmove(fnName, "Fn ", 3);
            memmove(fnName + 3, name.start, name.length);
            var.index = getIndexFromSymbolTable(&cu->curParser->curModule->moduleVarName, fnName, strlen(fnName));

            // 若不是函数名，那可能是该模块变量定义在引用处的后面
            // 先将行号作为该变量值去声明
            if (var.index == -1) {
                var.index = declareModuleVar(cu->curParser->vm, cu->curParser->curModule, name.start, name.length,
                                             NUM_TO_VALUE(cu->curParser->curToken.lineNo));
            }
        }
        emitLoadOrStoreVariable(cu, canAssign, var);
    }
}


// 中缀运算符 .led 方法
static void infixOperator(CompileUnit *cu, bool canAssign UNUSED) {
    SymbolBindRule *rule = &Rules[cu->curParser->preToken.type];

    // 中缀运算符对左右操作数的绑定权值一样
    BindPower rbp = rule->lbp;
    expression(cu, rbp); // 解析右操作数

    // 生成一个参数的签名
    Signature sign = {SIGN_METHOD, rule->id, strlen(rule->id), 1};
    emitCallBySignature(cu, &sign, OPCODE_CALL0);
}

// 前缀运算符 .nud 方法 如 -、！ 等
static void unaryOperator(CompileUnit *cu, bool canAssign UNUSED) {
    SymbolBindRule *rule = &Rules[cu->curParser->preToken.type];

    // BP_UNARY 作为rbp去调用expression解析右操作数
    expression(cu, BP_UNARY);

    // 生成调用前缀运算符的指令
    // 0个参数，前缀运算符都是1个字符，长度是1
    emitCall(cu, 0, rule->id, 1);
}

// 为中缀运算符创建签名
static void infixMethodSignature(CompileUnit *cu, Signature *sign) {
    // 在类中的运算符都是方法，类型为 SIGN_METHOD
    sign->type = SIGN_METHOD;

    // 中缀运算符法之后一个参数，故初始为1
    sign->argNum = 1;
    consumeCurToken(cu->curParser, TOKEN_LEFT_PAREN, "expect '(' after infix operator!");
    consumeCurToken(cu->curParser, TOKEN_ID, "expect variable name!");
    consumeCurToken(cu->curParser, TOKEN_RIGHT_PAREN, "expect ')' after parameter!");
}

// 语法分析的核心
static void expression(CompileUnit *cu, BindPower rbp) {
    // 以中缀运算符表达式 aSwTe 为例
    // 大写字符表示运算符，小写字符表示操作数

    // 进入 expression时，curToken 是操作数 w, preToken 是运算符 S
    DenotationFn nud = Rules[cu->curParser->curToken.type].nud;

    // 表达式开头的要么是操作数要么是前缀运算符，必然有nud方法
    ASSERT(nud != NULL, "nud is NULL!");

    getNextToken(cu->curParser); // 执行后curToken为运算符T

    bool canAssign = rbp < BP_ASSIGN;
    nud(cu, canAssign); // 计算操作数 w 的值
    // tokenType 只有 无效，数字，字符串的在Rules中有对应的下标
    while (rbp < Rules[cu->curParser->curToken.type].lbp) {
        DenotationFn led = Rules[cu->curParser->curToken.type].led;
        getNextToken(cu->curParser); // 执行后 curToken为操作数e
        led(cu, canAssign); // 计算运算符 T.led 方法
    }
}


// 在模块 objModule 中定义名称为name，值为value的模块变量
int defineModuleVar(VM *vm, ObjModule *objModule, const char *name, uint32_t length, Value value) {
    if (length > MAX_ID_LEN) {
        // 也许name指向的变量名并不以\0结束，将其从源码串中拷贝出来
        char id[MAX_ID_LEN] = {'\0'};
        memcpy(id, name, length);

        // 本函数可能是在编译源码文件之前调用
        // 那时还没有创建 parser 因此报错要分情况
        if (vm->curParser != NULL) {
            // 编译源码文件
            COMPILE_ERROR(vm->curParser, "length of identifer \"%s\" should be no more than %d", id, MAX_ID_LEN);
        } else {
            // 在编译源码前调用，比如加载核心模块时会调用本函数
            MEM_ERROR("length of identifer \"%s\" should be no more than %d", id, MAX_ID_LEN);
        }
    }
    // 从模块变量名中查找变量，若不存在就添加
    int symbolIndex = getIndexFromSymbolTable(&objModule->moduleVarName, name, length);
    if (symbolIndex == -1) {
        // 添加变量名
        symbolIndex = addSymbol(vm, &objModule->moduleVarName, name, length);
        // 添加变量值
        ValueBufferAdd(vm, &objModule->moduleVarValue, value);
    } else if (VALUE_IS_NUM(objModule->moduleVarValue.datas[symbolIndex])) {
        // 若遇到之前预先声明的模块变量的定义，再次为其赋予正确的值
        objModule->moduleVarValue.datas[symbolIndex] = value;
    } else {
        // 已定义则返回-1， 用于判断重定义
        symbolIndex = -1;
    }
    return symbolIndex;
}

//定义变量为其赋值
static void defineVariable(CompileUnit *cu, uint32_t index) {
    //局部变量已存储到栈中,无须处理.
    //模块变量并不存储到栈中,因此将其写回相应位置
    if (cu->scopeDepth == -1) {
        //把栈顶数据存入参数index指定的全局模块变量
        writeOpCodeShortOperand(cu, OPCODE_STORE_MODULE_VAR, index);
        writeOpCode(cu, OPCODE_POP);  //弹出栈顶数据,因为上面OPCODE_STORE_MODULE_VAR已经将其存储了
    }
}

//从局部变量,upvalue和模块中查找变量name
static Variable findVariable(CompileUnit *cu, const char *name, uint32_t length) {

    //先从局部变量和upvalue中查找
    Variable var = getVarFromLocalOrUpvalue(cu, name, length);
    if (var.index != -1) return var;

    //若未找到再从模块变量中查找
    var.index = getIndexFromSymbolTable(
            &cu->curParser->curModule->moduleVarName, name, length);
    if (var.index != -1) {
        var.scopeType = VAR_SCOPE_MODULE;
    }
    return var;
}
