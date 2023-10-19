#include <stdlib.h>
#include "./vm.h"
#include "../include/utils.h"

// 初始化虚拟机
void initVM(VM* vm){
    vm->allocatedBytes = 0;
    vm->curParser = NULL;
}

// 初始化虚拟机
VM* newVM(){
    VM* vm = (VM*)malloc(sizeof(VM));
    if(vm == NULL){
        MEM_ERROR("allocate VM failed!");
    }
    initVM(vm);
    return vm;
}


