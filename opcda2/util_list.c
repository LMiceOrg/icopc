/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "util_list.h"

#include <stddef.h>

#include <objbase.h>

int opcda2_list_size(util_list_head* l) {
    int sz = 0;

    while (l != NULL) {
        sz += l->size;
        l = l->next;
    }

    return sz;
}

#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })

void* opcda2_list_alloc(int item_size, int cap) {
    util_list_head *_lst;
    const int _sz = sizeof(util_list_head) +
                    (8 + item_size) * (cap);
    _lst = (util_list_head *)CoTaskMemAlloc(_sz);
    memset(_lst, 0, _sz);
    _lst->capacity = cap;
    _lst->item_size = item_size;
    _lst->insert_blob = container_of(_lst, util_list_head, next);

    return _lst;
}
