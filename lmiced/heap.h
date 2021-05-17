/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com>
* \file heap.h
* \brief 共享内存堆管理
* \details 堆由多个共享内存块组成，第一个共享内存块也称主内存块；主共享内存块起始位置是一个数组链表，存储
* 的初始大小固定
* \author He Hao
* \version 1.0
* \date 2021-05-10
* \since
* \pre 无先决条件
* \bug 未发现
* \warning 无警告
*/

#ifndef HEAP_H_
#define HEAP_H_

#include "lmice.h"


/** 创建堆
 * @param size 堆的初始大小 字节数
 * @return 堆句柄
 */
mihandle miheap_create(int size);

/** 删除堆
 * @param heap 堆句柄
 */
void miheap_release(mihandle heap);

/** 申请堆内存
 * @param heap 堆句柄
 * @param size 内存字节数
 * @return 指向分配的内存地址， 0: 无法分配内存
 */
void* miheap_malloc(mihandle heap, miu32 size);

/** 释放内存
 * @param heap 堆句柄
 * @param ptr 已分配的内存地址
 */
void miheap_free(mihandle heap, void* ptr);

/** 重新分配内存
 * @param heap 堆句柄
 * @param ptr 已分配的内存地址
 * @param size 新的字节数
 * @return 指向分配的内存地址， 0: 无法分配内存
 */
void* miheap_realloc(mihandle heap, void* ptr, miu32 size);

#endif  /**  HEAP_H_ */
