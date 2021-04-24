/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_UTIL_LIST_H_
#define OPCDA2_UTIL_LIST_H_

typedef struct util_list_head;

typedef struct util_list_blob {
  int used;
  int capacity;
  char data[8];
} util_list_blob;

typedef struct util_list_head {
  int size;
  int capacity;
  int item_size;
  int insert_pos;
  struct util_list_blob *insert_blob;
  struct util_list_head *next;
} util_list_head;

// #define opcda2_list_alloc(pt, tp, cap)                                         \
//   do {                                                                         \
//     util_list_head *_lst; \
//     const int _sz = sizeof(util_list_head) +                                   \
//                     (sizeof(util_list_blob) + sizeof(tp)) * (cap);             \
//     _lst = (util_list_head *)CoTaskMemAlloc(_sz);                                          \
//     memset(_lst, 0, _sz);                                                      \
//     _lst->capacity = (cap); \
//     _lst->item_size = sizeof(tp); \
//     _lst->insert_blob = container_of(_lst, util_list_head, next);\
//   }                                                                            \
//   whilw(0)

#define opcda2_list_free(pt) CoTaskMemFree(pt)

int opcda2_list_size(util_list_head *l);

#define opcda2_list_add(pt, tp, obj_list, obj_size)                            \
  do {                                                                         \
                                                                               \
  } while (0)

#define opcda2_list_del(pt, tp, obj)                                           \
  do {                                                                         \
                                                                               \
  } while (0)

#define list_init(pt, tp, cap)                                                 \
  do {                                                                         \
    const int _sz = sizeof(util_list_head) + sizeof(tp) * (cap);               \
    (pt) = (tp *)malloc(_sz);                                                  \
    memset((pt), 0, _sz);                                                      \
  }                                                                            \
  whilw(0)

#define list_free(pt) free(pt)

#endif /** OPCDA2_UTIL_LIST_H_ */
