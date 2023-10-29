#include "unicodeUtf8.h"
#include "common.h"
#include <stdint.h>

/**
 * Unicode 符号范围                        UTF-8 编码方式
 * [0000 0000 - 0000 007F]                0_______
 * (0000 0080 - 0000 07FF]                110_____ 10______
 * (0000 0800 - 0000 FFFF]                1110____ 10______ 10______
 * (0001 0000 - 0010 FFFF]                11110___ 10______ 10______ 10______
 */

// 返回value 按照 UTF-8 编码后的字节数量
uint32_t getByteNumOfEncodeUtf8(int value){
    ASSERT(value > 0, "Can't encode negative value!");

    // 单个 ASCII 字符需要1字节
    if(value <= 0x7f){
        // 0x7f -> 0111 1111
        return 1;
    }
    // 此范围内数值编码为UTF-8 需要2字节
    if(value <= 0x7ff){
        // 0x7ff -> 0000 0111 1111 1111
        return 2;
    }
    // 此范围内数值编码为UTF-8 需要3字节
    if(value <= 0xffff){
        // 0xffff ->  1111 1111 1111 1111
        return 3;
    }
     // 此范围内数值编码为UTF-8 需要4字节
    if(value <= 0x10ffff){
        // 0x10ffff -> 0001 0000 1111 1111 1111 1111
        return 4;
    }
    return 0; // 超过取数范围
}

// 把 value 编码为 UTF-8 后写入缓冲区 buf, 返回写入的字节数
uint8_t encodeUtf8(uint8_t* buf, int value){
    ASSERT(value > 0l, "Can't encode negative value!");

    // 按照大端字节序列写入缓冲区
    if(value <= 0x7f){ // 单个ASCII 字符需要一个字节
        *buf = value & 0x7ff;
        return 1;
    }else if(value <= 0x7ff){ // 此范围内数字编码为 UTF-8 需要2字节
        // 110_____ 10______
        // 0xc0     0x80

        // 0x7c0    -> 0000 0111 1100 0000
        // 先写入高字节 5位
        *buf++ = 0xc0 | ((value & 0x7c0) >> 6);
        // 0x3f     -> 0000 0000 0011 1111
        // 再写入低字节 6位
        *buf = 0x80 | (value & 0x3f);
        return 2;
    }else if(value <= 0xffff ){ // 此范围内数值编码为 UTF-8 需要3字节
        // 1110____ 10______ 10______
        // 0xe0     0x80     0x80
        // 0xf000  ->   0000 0000 1111 0000 0000 0000
        // 先写入高字节 4位
        *buf++ = 0xe0 | ((value & 0xf000) >> 12);
        // 0xfc0    ->  0000 0000 0000 1111 1100 0000
        // 写入中间字节 6位
        *buf++ = 0x80 | ((value & 0xfc0) >> 6) ;
        // 0x3f     ->  0000 0000 0000 0000 0011 1111
        // 写入低字节 6位
        *buf = 0x80 | (value & 0x3f);
        return 3;
    }else if(value <= 0x10ffff){ // 此范围内数值编码为UTF-8 需要4字节
        // 11110___ 10______ 10______ 10______

        // 高3位
        // 0x1c0000     -> 0000 0000 0001 1100 0000 0000 0000 0000
        *buf++ = 0xf0 | ((value & 0x1c0000) >> 18);
        // 中6位
        // 0x3f000      -> 0000 0000 0000 0011 1111 0000 0000 0000
        *buf++ = 0x80 | ((value & 0x3f000 ) >> 12);
        // 中6位
        // 0xfc0        -> 0000 0000 0000 0000 0000 1111 1100 0000
        *buf++ = 0x80 | ((value & 0xfc0   ) >> 6);
        // 最后6位
        // 0x3f         -> 0000 0000 0000 0000 0000 0000 0011 1111
        *buf   = 0x80 | (value & 0x3f);
        return 4;
    }
    NOT_REACHED();
    return 0;
}


// 返回解码 UTF-8 的字节数 byte 一般是第一个字节
uint32_t getByteNumOfDecodeUtf8(uint8_t byte){
    // byte 应该是 UTF-8 的最高字节，如果指向了 UTF-8 编码后面的低字节部分则返回0
    if((byte & 0xc0) == 0x80){
        // 0xc0 -> 1100 0000
        // 0x80 -> 1000 0000
        return 0;
    }
    if((byte & 0xf8) == 0xf0){
        // 0xf0 -> 1111 000
        return 4;
    }
    if((byte & 0xf0) == 0xe0){
        // 0xe0 -> 1110 0000
        return 3;
    }
    if((byte & 0xe0) == 0xc0){
        // 0xc0 -> 1100 0000
        return 2;
    }
    return 1; // ascii码
}

// 解码以 bytePtr 以起始地址的 UTF-8 序列，其最大长度为 length， 若不是 UTF-8 序列就返回 -1
int decodeUtf8(const uint8_t* bytePtr, uint32_t length){
    // 若1字节的 ascii：0_______
    if(*bytePtr <= 0x7f){
        return *bytePtr;
    }

    int value;
    uint32_t remainingBytes;

    // 先读取最高1字节
    // 根据最高字节的高n位判断响应字节数的 UTF-8 编码
    if((*bytePtr & 0xe0) == 0xc0){
        // 2字节的 utf8
        // 0xe0 -> 1110 0000
        // 0xc0 -> 1100 0000
        // 取5位
        value = *bytePtr & 0x1f;
        remainingBytes = 1;
    }else if((*bytePtr & 0xf0) == 0xe0){
        // 3字节的 utf8
        // 0xf0  -> 1111 0000
        // 0xe0  -> 1110 0000
        value = *bytePtr & 0x0f;
        remainingBytes = 2;
    }else if((*bytePtr & 0xf8) == 0xf0){
        // 4字节
        // 0xf8  -> 1111 1000
        // 0xf0  -> 1111 0000
        value = *bytePtr & 0x07;
        remainingBytes = 3;
    }else{
        // 非法编码
        return -1;
    }

    // 如果 UTF-8 被斩断了就不在读过去了
    if(remainingBytes > length - 1){
        return -1;
    }

    // 再读取低字节中的数据
    while(remainingBytes > 0){
        bytePtr++;
        remainingBytes--;
        // 高2位必须是10
        if((*bytePtr & 0xc0) != 0x80){
            return -1;
        }
        // 从次高字节往低字节，不断累加各字节的低6位
        value = value << 6 | (*bytePtr & 0x3f);
    }
    return value;
}















