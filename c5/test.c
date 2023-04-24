
#include <stdio.h>

// 为定义在opcode.inc 中的操作码加上前缀OPCODE_
#define OPCODE_SLOTS(opcode, effect) OPCODE_##opcode,
typedef enum {
   #include "opcode.inc"
} OpCode;
#undef OPCODE_SLOTS

int main(int argc, char const *argv[])
{
    OpCode OPCODE_LOAD_CONSTANT;
    printf("%u",OpCode[0]);
    return 0;
}
