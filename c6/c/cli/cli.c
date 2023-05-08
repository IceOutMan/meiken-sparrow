#include "cli.h"
#include <stdio.h>
#include <string.h>
#include "../parser/parser.h"
#include "../vm/vm.h"
#include "../vm/core.h"
#include "../object/class.h"


// 执行脚本文件
static void runFile(const char* path){
    // strrchr 找到最后一次出现 / 的位置
    // 文件路径复制懂啊rootDir变量中
    const char* lastSlash = strrchr(path, '/');
    if(lastSlash != NULL){
        char* root = (char*)malloc(lastSlash - path + 2);
        memcpy(root, path, lastSlash - path + 1);
        root[lastSlash - path + 1] = '\0';
        rootDir = root;
    }

    // VM
    VM* vm = newVM();
    const char* sourceCode = readFile(path);
    // 作为测试使用
    // parseAndPrintToken(vm, path, sourceCode);

    executeModule(vm, OBJ_TO_VALUE(newObjString(vm, path, strlen(path))), sourceCode);
  
   
}

int main(int argc, const char** argv){
    if(argc == 1){
        ;
    }else{
        runFile(argv[1]);
    }

    return 0;
}