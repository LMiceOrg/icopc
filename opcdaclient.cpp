#include "stdafx.h"
#include "opcdaclient.h"

#include "call_thunk/call_thunk.h"

#define OPC_GROUP0_NAME "IC_OPCDACLIENT group0"

ic_opcdaclient::ic_opcdaclient() {
m_opc=NULL;
m_thunk = new call_thunk::unsafe_thunk(6, NULL, call_thunk::cc_stdcall, call_thunk::cc_stdcall);

}

ic_opcdaclient::~ic_opcdaclient(){

        //RemoveOPCGroup(m_opc, m_group);
    
    if(m_opc) {
        DisconnectOPC(m_opc);
    }
    if(m_thunk) {
        delete m_thunk;
    }
}

void ic_opcdaclient::connect(const char* host, const char* name) {
    if(m_opc) {
        DisconnectOPC(m_opc);
    }
    m_opc= ConnectOPC(host, name, TRUE);
    
    add_group(OPC_GROUP0_NAME, 5000, 0);

 
    m_thunk->bind(*this, &ic_opcdaclient::update_proc);
    opc_update_proc proc;
    proc = reinterpret_cast<opc_update_proc>(m_thunk->code());;
    EnableOPCNotificationEx(m_opc, proc);
}

int ic_opcdaclient::add_group(const char* name, unsigned long rate, float dead_band){
    HANDLE group = AddOPCGroup(m_opc, name, &rate, &dead_band);
    if(group) {
        opc_group grp;
        grp.group = group;
        grp.name = name;
        grp.opc = m_opc;
        grp.rate = rate;
        grp.dead_band = dead_band;

        // ²åÈë ×é
        m_groups.push_back(grp);
    } else {
        return -2;
    }
    int sz = m_groups.size();
    return sz -1;
}

int ic_opcdaclient::add_item(const char* name, int grp) {
    opc_item item;
    opc_group *pg;

    if(grp >= m_groups.size()) {
        return -1;
    }
    pg = &m_groups[grp];
    
    item.item = AddOPCItem(m_opc, pg->group, name);
    if(!item.item) {
        return -2;
    }

    item.name = name;
    item.group = pg->group;

    // ²åÈë
    m_items.push_back(item);
    int sz = m_items.size();
    return sz-1;
}
void CALLBACK 
ic_opcdaclient::update_proc(HANDLE hConnection, HANDLE hGroup, 
                            HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality) {
char buf[512];
    size_t sz;
    memset(buf, 0, sizeof(buf));
    GetOPCItemNameFromHandle(hConnection, hGroup, hItem, buf, sizeof(buf));
    sz = strlen(buf);
    sprintf(buf + sz, " type[%d] = ", pVar->vt);
    sz = strlen(buf);

    CComVariant var(*pVar);

    switch(var.vt){
        case VT_EMPTY:
		case VT_NULL:
            break;
        case VT_BOOL:
            sprintf(buf+sz, "VT_BOOL %d", var.boolVal);
            break;
        case VT_UI1: 
            sprintf(buf+sz, "VT_UI1 %d", var.bVal);
            break;
        case VT_I2: 
            sprintf(buf+sz, "VT_I2 %d", var.iVal);
            break;
        case VT_I4:
            sprintf(buf+sz, "VT_I4 %d", var.lVal);
            break;
        case VT_R4:
            sprintf(buf+sz, "VT_R4 %f", var.fltVal);
            break;
        case VT_R8:
            sprintf(buf+sz, "VT_R8 %lf", var.dblVal);
            break;
        case VT_BSTR:
            sprintf(buf+sz, "VT_BSTR"); //, var.bstrVal
            break;
        case VT_ERROR:
            sprintf(buf+sz, "VT_ERROR %d", var.scode);
            break;
        case VT_DISPATCH:
            sprintf(buf+sz, "VT_DISPATCH"); //var.pdispVal;
            break;
        case VT_UNKNOWN:
            sprintf(buf+sz, "VT_UNKNOWN"); //var.punkVal;
            break;
        default:
            break;
    }
    MessageBoxA(NULL, buf, NULL, MB_OK);
}

void CALLBACK opc_update_proc_1(HANDLE hConnection, HANDLE hGroup, HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality) {
    
    char buf[512];
    size_t sz;
    memset(buf, 0, sizeof(buf));
    GetOPCItemNameFromHandle(hConnection, hGroup, hItem, buf, sizeof(buf));
    sz = strlen(buf);
    sprintf(buf + sz, " type[%d] = ", pVar->vt);
    sz = strlen(buf);

    CComVariant var(*pVar);

    switch(var.vt){
        case VT_EMPTY:
		case VT_NULL:
            break;
        case VT_BOOL:
            sprintf(buf+sz, "VT_BOOL %d", var.boolVal);
            break;
        case VT_UI1: 
            sprintf(buf+sz, "VT_UI1 %d", var.bVal);
            break;
        case VT_I2: 
            sprintf(buf+sz, "VT_I2 %d", var.iVal);
            break;
        case VT_I4:
            sprintf(buf+sz, "VT_I4 %d", var.lVal);
            break;
        case VT_R4:
            sprintf(buf+sz, "VT_R4 %f", var.fltVal);
            break;
        case VT_R8:
            sprintf(buf+sz, "VT_R8 %lf", var.dblVal);
            break;
        case VT_BSTR:
            sprintf(buf+sz, "VT_BSTR"); //, var.bstrVal
            break;
        case VT_ERROR:
            sprintf(buf+sz, "VT_ERROR %d", var.scode);
            break;
        case VT_DISPATCH:
            sprintf(buf+sz, "VT_DISPATCH"); //var.pdispVal;
            break;
        case VT_UNKNOWN:
            sprintf(buf+sz, "VT_UNKNOWN"); //var.punkVal;
            break;
        default:
            break;
    }
    MessageBoxA(NULL, buf, NULL, MB_OK);
}