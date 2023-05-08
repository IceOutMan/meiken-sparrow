#ifndef _VM_CORE_H
#define _VM_CORE_H
// rootDir 是在别处定义，这里需要使用到, 所以使用extern
extern char* rootDir;

char* readFile(const char* sourceFile);

#endif