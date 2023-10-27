#include "obj_map.h"
#include "../vm/vm.h"
#include "class.h"
#include "obj_range.h"


// 创建新的 map 对象
ObjMap *newObjMap(VM *vm) {
    ObjMap *objMap = ALLOCATE(vm, ObjMap);
    initObjHeader(vm, &objMap->objHeader, OT_MAP, vm->mapClass);
    objMap->capacity = objMap->count = 0;
    objMap->entries = NULL;
    return objMap;
}


// 计算数字的哈希码
static uint32_t hashNum(double num) {
    Bits64 bits64;
    bits64.num = num;
    return bits64.bits32[0] & bits64.bits32[1];
}

// 计算对象的哈希码
static uint32_t hashObj(ObjHeader *objHeader) {
    switch (objHeader->type) {
        case OT_CLASS: // 计算 class 的哈希值
            return hashString(((Class *) objHeader)->name->value.start, ((Class *) objHeader)->name->value.length);
        case OT_RANGE: {// 计算 range 对象哈希码
            ObjRange *objRange = (ObjRange *) objHeader;
            return hashNum(objRange->from) ^ hashNum(objRange->to);
        }
        case OT_STRING: // 对于字符串，直接返回其 hashCode
            return ((ObjString *) objHeader)->hashCode;
        default:
            RUN_ERROR("the hashtable are objstring, objrange and calsss.");
    }
    return 0;
}

// 根据 value 的类型调用相应的哈希函数
static uint32_t hashValue(Value value) {
    switch (value.type) {
        case VT_FALSE:
            return 0;
        case VT_NULL:
            return 1;
        case VT_NUM:
            return hashNum(value.num);
        case VT_TRUE:
            return 2;
        case VT_OBJ:
            return hashObj(value.objHeader);
        default:
            RUN_ERROR("unsupport type hashed!");
    }
}

// 在 entries 中添加 entry，如果是新的 key 则返回 true
static bool addEntry(Entry *entries, uint32_t capacity, Value key, Value value) {
    uint32_t index = hashValue(key) % capacity;

    // 通过开放探测法去找可用的 slot
    while (true) {
        // 找到空闲的 slot，说明目前没有此 key，直接赋值 返回
        if (entries[index].key.type == VT_UNDEFINED) {
            entries[index].key = key;
            entries[index].value = value;
            return true; // 新的 key 就返回 true
        } else if (valueIsEqual(entries[index].key, key)) {
            // key 已经存在，仅仅更新值即可
            entries[index].value = value;
            return false;   // 未增加新的 key 返回 false
        }
        // 开放探测地址，尝试下一个 slot
        index = (index + 1) % capacity;
    }
}

// 使对象 objMap 的容量调整到 newCapacity
static void resizeMap(VM *vm, ObjMap *objMap, uint32_t newCapacity) {
    // 1. 建立一个新的 entry 数组
    Entry *newEntries = ALLOCATE_ARRAY(vm, Entry, newCapacity);
    uint32_t idx = 0;
    while (idx < newCapacity) {
        newEntries[idx].key = VT_TO_VALUE(VT_UNDEFINED);
        newEntries[idx].value = VT_TO_VALUE(VT_FALSE);
        idx++;
    }

    // 2. 遍历老的数组，把有值的部分插入到新数组
    if (objMap->capacity > 0) {
        Entry *entryArr = objMap->entries;
        idx = 0;
        while (idx < objMap->capacity) {
            // 该 slot 有值
            if (entryArr[idx].key.type != VT_UNDEFINED) {
                addEntry(newEntries, newCapacity, entryArr[idx].key, entryArr[idx].value);
            }
            idx++;
        }
    }

    // 3. 将老 entry 数组清空回收
    DEALLOCATE_ARRAY(vm, objMap->entries, objMap->count);
    objMap->entries = newEntries;           // 更新指针为新的 entry 数组
    objMap->capacity = newCapacity;         // 更新容量
}

// 在 objMap 中查找 key 对应的 entry
static Entry *findEntry(ObjMap *objMap, Value key) {
    // objMap 为空则返回 null
    if (objMap->capacity == 0) {
        return NULL;
    }
    // 以下开放地址探测
    // 用哈希值对容量取模计算槽位(slot)
    uint32_t index = hashValue(key) % objMap->capacity;
    Entry *entry;
    while (true) {
        entry = &objMap->entries[index];

        // 若该 slot 中的 entry 正好是该 key 的 entry，找到返回
        if (valueIsEqual(entry->key, key)) {
            return entry;
        }

        // key 为 VT_UNDEFINED 且 value 为 VT_TRUE 表示探测连未断，可继续探测
        // key 为 VT_UNDEFINED 且 value 为 VT_FALSE 表示探测连结束，探测结束
        if (VALUE_IS_UNDEFINED(entry->key) && VALUE_IS_FALSE(entry->value)) {
            return NULL; // 未找到
        }
        // 继续探测
        index = (index + 1) % objMap->capacity;
    }
}

// 在 objMap 中实现 key 和 value 的关联： objMap[key] = value
void mapSet(VM *vm, ObjMap *objMap, Value key, Value value) {
    // 当容量利用率达到 80% 时扩容
    if (objMap->count + 1 > objMap->capacity * MAP_LOAD_PERCENT) {
        uint32_t newCapacity = objMap->capacity * CAPACITY_GROW_FACTOR;
        if (newCapacity < MIN_CAPACITY) {
            newCapacity = MIN_CAPACITY;
        }
        resizeMap(vm, objMap, newCapacity);
    }

    // 若创建了新的 key 则使 objMap->count 加1
    if (addEntry(objMap->entries, objMap->capacity, key, value)) {
        objMap->count++;
    }

}

// 从 map 中查找 key 对应的 value:map[key]
Value mapGet(ObjMap *objMap, Value key) {
    Entry *entry = findEntry(objMap, key);
    if (entry == NULL) {
        return VT_TO_VALUE(VT_UNDEFINED);
    }
    return entry->value;
}

// 回收 objMap.entries 占用的空间
void clearMap(VM *vm, ObjMap *objMap) {
    DEALLOCATE_ARRAY(vm, objMap->entries, objMap->count);
    objMap->entries = NULL;
    objMap->capacity = objMap->count = 0;
}

// 删除 objMap 中的 key 返回 map[key]
Value removeKey(VM *vm, ObjMap *objMap, Value key) {
    Entry *entry = findEntry(objMap, key);

    if (entry == NULL) {
        return VT_TO_VALUE(VT_NULL);
    }

    // 设置开放地址的伪删除
    Value value = entry->value;
    entry->key = VT_TO_VALUE(VT_UNDEFINED);
    entry->value = VT_TO_VALUE(VT_TRUE); // 值为真，伪删除标记

    objMap->count--;
    if (objMap->count == 0) { // 若删除该 entry 后 map 为空就回收该空间
        clearMap(vm, objMap);
    } else if (objMap->count < objMap->capacity / (CAPACITY_GROW_FACTOR) * MAP_LOAD_PERCENT
               && objMap->count > MIN_CAPACITY) {
        // 若 map 容量利用率太低，就缩小 map 空间
        uint32_t newCapacity = objMap->capacity / CAPACITY_GROW_FACTOR;
        if(newCapacity < MIN_CAPACITY){
            newCapacity = MIN_CAPACITY;
        }
        resizeMap(vm, objMap, newCapacity);
    }

    return value;
}

