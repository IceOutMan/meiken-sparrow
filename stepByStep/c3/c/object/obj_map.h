#ifndef _OBJECT_MAP_H
#define _OBJECT_MAP_H

#include "header_obj.h"

#define MAP_LOAD_PERCENT 0.8

typedef struct {
    Value key;
    Value value;
} Entry; // key->value对儿

typedef struct {
    ObjHeader objHeader;
    uint32_t capacity; // Entry 的容量（既总数），包括已使用和未使用 Entry 的数量
    uint32_t count; // map 中使用的 Entry 的数量
    Entry* entries; // Entry 数组
} ObjMap;

ObjMap* newObjMap(VM* vm);

void mapSet(VM* vm, ObjMap* objMap, Value key, Value value);
Value mapGet(ObjMap* objMap, Value key);
void clearMap(VM* vm, ObjMap* objMap);

#endif