#ifndef _OPC_DA_CLIENT_H
#define _OPC_DA_CLIENT_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <stdio.h>
#include <string.h>

#include "call_thunk/call_thunk.h"

#ifdef __cplusplus

#include <vector>
#include <map>
#include <string>

struct opc_group {
HANDLE group;
HANDLE opc;
std::string name;
unsigned long rate;
float dead_band;
};

struct opc_item{
    HANDLE item;
    HANDLE group;
    std::string name;
    CComVariant value;
    DWORD qulity;
    FILETIME time;
};

class ic_opcdaclient {
public:
    ic_opcdaclient();
    ~ic_opcdaclient();

    void connect(const char* host, const char* name);
    int add_group(const char* name, unsigned long rate, float dead_band);
    int add_item(const char* name, int group=0);
    void CALLBACK update_proc(HANDLE hConnection, HANDLE hGroup, 
        HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality);
private:
    HANDLE m_opc;
    std::vector<opc_group> m_groups;
    std::vector<opc_item>m_items;
    call_thunk::unsafe_thunk* m_thunk;
};
typedef void  (CALLBACK * opc_update_proc)(HANDLE, HANDLE, 
        HANDLE, VARIANT *, FILETIME, DWORD);

extern "C" {
#endif

    void CALLBACK opc_update_proc_1(HANDLE hConnection, HANDLE hGroup, 
        HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality);
#ifdef __cplusplus
}
#endif

#endif // _OPC_DA_CLIENT_H