#ifndef _VM_CORE_H
#define _VM_CORE_H
#include "./vm.h"
#include "../object/class.h"
// rootDir 是在别处定义，这里需要使用到, 所以使用extern
extern char* rootDir;

char* readFile(const char* sourceFile);
void buildCore(VM* vm);
VMResult executeModule(VM* vm, Value moduleName, const char* moduleCode);
int addSymbol(VM* vm, SymbolTable* table, const char* symbol, uint32_t length);
int getIndexFromSymbolTable(SymbolTable* table, const char* symbol, uint32_t length);
void bindSuperClass(VM* vm, Class* subClass, Class* superClass);
void bindMethod(VM* vm, Class* class, uint32_t index, Method method);
int ensureSymbolExist(VM* vm, SymbolTable* table, const char* symbol, uint32_t length);

#endif