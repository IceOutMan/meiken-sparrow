#ifndef _CLI_CLI_H
#define _CLI_CLI_H
#include "../vm/vm.h"

#define VERSION 0.1.0
#define MAX_LINE_LEN 1024

Class* newRawClass(VM* vm, const char* name, uint32_t fieldNum);
#endif