#ifndef _VM_CORE_H
#define _VM_CORE_H
#include "vm.h"
// extern 代表, rootDir  是外部变量
// 当其他文件使用 core.h 文件时候,就会用到 下面这句话
// 相当于是在其他文件中可以使用在 core.a 中定义的全局变量 rootDirt
extern char* rootDir;
int defineModuleVar(VM* vm, ObjModule* objModule, const char* name, uint32_t length, Value value);
VMResult executeModule(VM* vm, Value moduleName, const char* moduleCode);
int addSymbol(VM *vm, SymbolTable *table, const char *symbol, uint32_t length);
int ensureSymbolExist(VM* vm, SymbolTable* table, const char* symbol, uint32_t length);

int getIndexFromSymbolTable(SymbolTable *table, const char *symbol, uint32_t length);


void buildCore(VM* vm);
char* readFile(const char* sourceFile);

#endif
