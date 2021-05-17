/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com>
* \file lmice.h
* \brief 简要介绍
* \details 详细说明
* \author He Hao
* \version 1.0
* \date 2021-05-10
* \since
* \pre 无先决条件
* \bug 未发现
* \warning 无警告
*/

#ifndef LMICE_H
#define LMICE_H

#include <stddef.h>

/* 服务访问接口(SPI Service Provider Interface)分多层
层0 c风格API
层1 C++风格API
层2 高层API
层3 JSON-RPC API


前缀mi(Lmilab)

*/

/* 类型定义 */

/** 对象/消息的类型 64bit 无符号整数 */
#define mitype unsigned __int64

/** 对象运行时实例 64bit 无符号整数 */
#define miinst unsigned __int64

/** 系统节拍 64bit 整数 */
#define mitick __int64

/** 资源索引(ID) 32bit 整数 */
#define miid int

#define miu8 unsigned char
#define miu16 unsigned __int16
#define miu32 unsigned int
#define miu64 unsigned __int64

#define mii8 char
#define mii16 __int16
#define mii32 int
#define mii64 __int64

/** 16比特 整数 */
#define miint16 __int16
/** 32比特 整数 */
#define miint32 int
/** 64比特 整数 */
#define miint64 __int64

/** UTF-16LE 字符 */
#define michar16 __int16

/** UTF-8 字符 */
#define michar8 char

/** 无符号 字符 */
#define miuchar8 unsigned char

/** C字符串类型 UTF-16LE 指针 */
#define miwstring michar16*

/** C字符串类型 UTF-8 指针 */
#define mistring michar8*

/** 句柄 指针 */
typedef union {
    miint32 i32;
    void*   ptr;
    miint64 i64;
} mihandle;

/** 节点类型 */
typedef enum MINODETYPE {
    LOCALNODE  = 0, /**< 本机节点 */
    REMOTENODE = 1  /**< 远程节点 */
} MINODETYPE;

/** 创建 服务访问接口
 * @param type 节点类型
 * @param name 节点名称
 * @remark 当节点类型未本机节点时, 节点名称可以为空
 * @return 服务访问接口
*/
mihandle spi_create(MINODETYPE type, const mistring name);

/** 释放 服务访问接口 */
void spi_release(mihandle spi);

#endif /* LMICE_H */

