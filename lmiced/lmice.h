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

#include "eal/lmice_eal_common.h"

/* 服务访问接口(SPI Service Procedure Interface)分多层
层0 c风格API
层1 C++风格API
层2 高层API
层3 JSON-RPC API


前缀mi(Lmilab)

是LMICE所提供服务的接口与约定，提供开发人员/应用软件基于LMICED的访问例程。

资源管理服务（消息资源，时间（定时器）资源）
任务调度服务（状态机， 离散事件（消息事件，时间事件，组合事件））
网络通讯服务（流入处理，流出处理）
可信计算服务（）


类比于Linux/Unix: 外物皆文件； LMICE：万物皆资源
资源具有时效性，结构性，主体性

时效性指同一件事物在不同的时间具有很大的性质上的差异，我们管这个差异性叫时效性。时效性影响着决策的生效时间，时效性决定了决策在特定时间内是否有效。
结构性定义是英国罗素用语。指如果我们要下定义的事物是一个由已知元素组成的结构，我们可以通过说出这些元素和借以构成这个结构的那些关系，给它下的定义。如果所下定义的事物是一个集合，那也可能只需说出它的结构，因为那些元素可能无关宏旨。
主体性指人在实践过程中表现出来的能力、作用、个人看法以及地位，即人的自主、主动、能动、自由、有目的地活动的地位和特性。
*/

/* 类型定义 */

#define miu8 unsigned char
#define miu16 unsigned short
#define miu32 unsigned int

#define mii8 char
#define mii16 short
#define mii32 int

#define miu64 ealu64
#define mii64 eali64


/** 对象/消息的类型 64bit 无符号整数 */
#define mitype miu64

/** 对象运行时实例 64bit 无符号整数 */
#define miinst miu64

/** 系统节拍 64bit 整数 */
#define mitick mii64

/** 资源索引(ID) 32bit 整数 */
#define miid int

/** 16比特 整数 */
#define miint16 __int16
/** 32比特 整数 */
#define miint32 int
/** 64比特 整数 */
#define miint64 mii64

/** UTF-16LE 字符 */
#define michar16 short

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
    double dval;
    miint32 i32;
    miint64 i64;
    void*   ptr;

    struct {
        miu32 high;
        miu32 low;
    } rid;

    struct {
        miu32 heap_id;
        miu32 node_id;
    } heap;
} mihandle;

struct mispi;
struct mipub_client;
struct node_head;
struct miboard;
struct mispi_event;

/** SPI枚举值 */
typedef enum {
    /* SPI类型 */
    MILOCAL     = 0,         /**< 本机节点 */
    MIREMOTE    = 1,         /**< 远程节点 */
    MIPRIVATE   = 2,         /**< 私有SPI模式, 仅本进程可用 */
    MIPUBLIC    = 4,         /**< 共有SPI模式, 本机所有进程可用 */
    MIIPC       = (0 << 8),  /**< 进程间通讯协议模式(仅本机有效) */
    MIWEBSOCKET = (1 << 8),  /**< Websocket协议通讯 */
    MITCP       = (2 << 8),  /**< TCP协议模式 */
    MIUDS       = (4 << 8),  /**< Unix Domain Socket 协议 */
    MIRAW       = (0 << 16), /**< 原始数据格式 */
    MIJSON      = (1 << 16), /**< json数据格式 */
    MISM4       = (2 << 16), /**< SM4加密数据格式 */
    MISM2       = (4 << 16), /**< SM2非对称加密数据格式 */

    /* SPI 事件 */
    MICLIENTCREATE     = 1, /**< 创建客户端 */
    MICLIENTRELEASE    = 2, /**< 释放客户端 */
    MICLIENTPUBCREATE  = 3, /**< 创建发布资源 */
    MICLIENTPUBRELEASE = 4, /**< 注销发布资源 */

    /* SPI 状态 */
    MICLOSED  = 0, /**< SPI未连接服务端 */
    MIRUNNING = 1, /**< SPI正常工作 */

    /* SPI 错误码 */
    MIOK = 0, /**< 正常 */

    /* 资源触发类型 */
    MIEVENTFIRE = 0,         /**< 由资源变化事件触发响应  */
    MITIMEFIRE  = (1 << 16), /**< 由物理时间触发响应, 16bit时间(单位毫秒) */
    MITICKFIRE  = (2 << 16), /**< 由逻辑节拍触发响应, 16bit节拍(单位tick) */

    /* 数据常量 */
    MIRESVARLENGTH = 0,    /**< 可变长类型 */
    MINAMESIZE     = 32,   /**< 名称字符串长度 */
    MIRESSIZE      = 65536, /**< 可变长资源的默认长度 */
    MICTIMEPERIOD  = 50   /**< 默认触发时间 */
} MISPIENUM;

/** 创建 服务访问接口
 * @param type SPI类型
 * @param name 节点名称 (type = MIREMOTE 时表示节点地址 ip/hostname)
 * @remark 当节点类型未本机节点时, 节点名称可以为空
 * @return 服务访问接口
 */
mihandle spi_create(miu32 type, const mistring name);

/** 释放 服务访问接口 */
void spi_release(mihandle spi);

/** ***** 发布服务 ***** */

/**
 * @brief 创建 发布资源
 * @param spi[in] 服务程序接口
 * @param t_name[in] 类型名称
 * @param o_name[in] 对象名称
 * @param size[in] 资源占用字节数 （=0 表示其长度可以动态扩展）
 * @param mode[in] 资源响应类型
 * @return 发布资源句柄
 */
mihandle spi_pub_create(mihandle spi, const mistring t_name, const mistring o_name, miu32 size, miu32 mode);

/**
 * @brief 释放 发布资源
 * @param pub[in] 发布资源
 */
void spi_pub_release(mihandle pub);

int spi_pub(mihandle pub, const void* ptr, miu32 size);

#endif /* LMICE_H */
