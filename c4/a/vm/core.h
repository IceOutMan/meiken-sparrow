#ifndef _VM_CORE_H
#define _VM_CORE_H

#include "../object/header_obj.h"
#include "../include/common.h"
#include "vm.h"

// rootDir 是在别处定义，这里需要使用到, 所以使用extern
extern char* rootDir;

VMResult executeModule(VM* vm, Value moduleName, const char* moduleCode);
char* readFile(const char* sourceFile);
void buildCore(VM* vm);

#endif