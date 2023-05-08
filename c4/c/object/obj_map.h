#ifndef _OBJECT_MAP_H
#define _OBJECT_MAP_H
#include "header_obj.h"

#define MAP_LOAD_PERCENT 0.8

typedef struct{
    Value key;
    Value value;
} Entry; // key-value 对儿

typedef struct{
    ObjHeader objHeader;
    uint32_t capacity; // Entry的容量（即总数），包括已使用和未使用的Entry的数量
    uint32_t count; // map中使用的Entry的数量
    Entry* entries; // Entry 数组
} ObjMap;

ObjMap* newObjMap(VM* vm);

void mapSet(VM* vm, ObjMap* objMap, Value key, Value value);
Value mapGet(ObjMap* objMap, Value key);
void clearMap(VM* vm, ObjMap* ObjMap);
Value removeKey(VM* vm, ObjMap* objMap, Value key);

#endif