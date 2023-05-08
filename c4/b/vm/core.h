#ifndef _VM_CORE_H
#define _VM_CORE_H

#include "../object/header_obj.h"
#include "../include/common.h"
#include "vm.h"

// rootDir 是在别处定义，这里需要使用到, 所以使用extern
extern char* rootDir;

char* readFile(const char* sourceFile);
void buildCore(VM* vm);
VMResult executeModule(VM* vm, Value moduleName, const char* moduleCode);
int getIndexFromSymbolTable(SymbolTable* table, const char* symbol, uint32_t length);
int addSymbol(VM* vm, SymbolTable* table, const char* symbol, uint32_t length);



#endif