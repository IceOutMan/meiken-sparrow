#include "cli.h"
#include <stdio.h>
#include <string.h>
#include "../parser/parser.h"
#include "../vm/vm.h"
#include "../vm/core.h"
#include "../object/class.h"

// 作为测试使用
// void parseAndPrintToken(VM* vm, const char* path, const char* sourceCode){
//      // parser
//     struct parser parser;
//     // 此NULL是临时的
//     initParser(vm, &parser, path ,sourceCode, NULL);
//
//     // 打印解析的TOKEN
//     printToken(parser);
//      // 还有这种骚操作
//     // tokenArray 和 TokenType 一一对应
//     #include "token.list"
//     while( parser.curToken.type != TOKEN_EOF ){
//         // 获取一个token
//         getNextToken(&parser);
//         // 打印 token 对应行号 和 类型标记
//         printf("%dL: %s, %d ,[", parser.curToken.lineNo, tokenArray[parser.curToken.type],parser.curToken.type);
//         // 打印 token 对应的字符内容
//         uint32_t idx = 0;
//         while(idx < parser.curToken.length){
//             printf("%c", *(parser.curToken.start+idx++));
//         }
//         printf("]\n");
//     }
//
//     // 测试代码
//     // for(int index = 0; index <= 31; index++){
//     //     printf("TOKNE_ARRAY: %d -  %s\n", index, tokenArray[index]);
//     // }
//
// }

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
    printf("There is something to do...\n");exit(0);

    const char* sourceCode = readFile(path);
    // 作为测试使用
    // parseAndPrintToken(vm, path, sourceCode);

    executeModule(vm, OBJ_TO_VALUE(newObjString(vm, path, strlen(path))), sourceCode);
  
   
}

int main(int argc, const char** argv){
//    if(argc == 1){
//        ;
//    }else{
//        runFile(argv[1]);
//    }
    char* path =  "../sample.sp";
    runFile(path);

    return 0;
}