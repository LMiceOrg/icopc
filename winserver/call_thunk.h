#ifndef _CALL_THUNK_H_
#define _CALL_THUNK_H_

//#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <exception>

/*
call C++ member function as C functions's callback
*/
#define intptr_t long
#define uintptr_t unsigned long

#define noexcept

#if defined(_M_IX86) || defined(__i386__)

#if defined(_MSC_VER)
#define __FASTCALL__ __fastcall
#define __CDECL__ __cdecl
#define __STDCALL__ __stdcall
#define __THISCALL__ __thiscall
#elif defined(__GNUC__)
#define __FASTCALL__ __attribute__((__fastcall__))
#define __CDECL__ __attribute__((__cdecl__))
#define __STDCALL__ __attribute__((__stdcall__))
#define __THISCALL__ __attribute__((__thiscall__))
#endif

#elif defined(_M_X64) || defined(__x86_64__)

#define __FASTCALL__
#define __CDECL__
#define __STDCALL__
#define __THISCALL__

#endif

namespace call_thunk {

class bad_call : std::exception {
 public:
  bad_call() noexcept {}
  virtual ~bad_call() noexcept {}
  virtual const char* what() const noexcept {
    return "call convention mismatch of function.";
  }
};

enum call_declare {
  cc_fastcall,  //__FASTCALL__
#if defined(_M_IX86) || defined(__i386__)
  cc_cdecl,     //_cdecl
  cc_stdcall,   //__STDCALL__
  cc_thiscall,  // thiscall
  default_caller = cc_cdecl,
#ifdef _WIN32
  default_callee = cc_thiscall
#else
  default_callee = cc_cdecl
#endif  //_MSC_VER
#elif defined(_M_X64) || defined(__x86_64__)
  default_caller = cc_fastcall,
  default_callee = cc_fastcall
#endif
};

struct thunk_code;

struct argument_info {
  short _size;
  bool _is_floating;

  argument_info() : _size(0), _is_floating(false) {}

#ifdef USE_CPP11
  template <typename T>
  void init() {
    if (std::is_pointer<T>::value || std::is_reference<T>::value)
      _size = sizeof(intptr_t);
    else
      _size = sizeof(T);
    _is_floating = std::is_floating_point<T>::value;
  }
#endif  // C++11

  bool as_integer() const;
  bool as_floating() const;
  short stack_size() const;
};

inline bool argument_info::as_integer() const {
#if defined(_M_IX86) || defined(__i386__)
  return stack_size() == sizeof(intptr_t);

#elif defined(_M_X64) || defined(__x86_64__)
#if defined(_WIN32)
  return _size == sizeof(char) || _size == sizeof(short) ||
         _size == sizeof(int) || _size == sizeof(intptr_t);
#else
  return !_is_floating && stack_size() == sizeof(intptr_t);
#endif

#endif
}

inline bool argument_info::as_floating() const { return _is_floating; }

inline short argument_info::stack_size() const {
  return (_size + sizeof(intptr_t) - 1) / sizeof(intptr_t) * sizeof(intptr_t);
}

class base_thunk {
 protected:
  base_thunk() : _code(NULL), _thunk_size(0), _thunk(NULL) {}
  ~base_thunk() { destroy_code(); }

  char* _code;

  void init_code(call_declare caller, call_declare callee, size_t argc,
                 const argument_info* arginfos = NULL) throw(bad_call);
  void destroy_code();
  void flush_cache();
  void bind_impl(void* object, void* proc);

 private:
  size_t _thunk_size;
  thunk_code* _thunk;
  base_thunk(const base_thunk&);
  base_thunk& operator=(const base_thunk&);
};

class unsafe_thunk : public base_thunk {
 public:
  explicit unsafe_thunk(size_t argc, const argument_info* arginfos = NULL,
                        call_declare caller = default_caller,
                        call_declare callee = default_callee) {
    init_code(caller, callee, argc, arginfos);
  }

  template <typename T, typename PROC>
  unsafe_thunk(T* object, PROC proc, size_t argc,
               const argument_info* arginfos = NULL,
               call_declare caller = default_caller,
               call_declare callee = default_callee) {
    init_code(caller, callee, argc, arginfos);
    bind(object, proc);
  }

  template <typename T, typename PROC>
  void bind(T& object, PROC proc) {
    bind_impl(static_cast<void*>(&object), *(void**)&proc);
    flush_cache();
  }

  template <typename Callback>
  operator Callback() const {
    return reinterpret_cast<Callback>(_code);
  }
  char* code() const {
      return _code;
  }
};


}  // namespace call_thunk

#endif  //_CALL_TRUNK_H_
