#include "stdafx.h"
#include "opcdaclient.h"

#include "call_thunk/call_thunk.h"

#define OPC_GROUP0_NAME "IC_OPCDACLIENT group0"

ic_opcdaclient::ic_opcdaclient() {
//m_opc=NULL;
//m_thunk = new call_thunk::unsafe_thunk(6, NULL, call_thunk::cc_stdcall, call_thunk::cc_stdcall);
InitializeCriticalSection(&m_lock);
}

ic_opcdaclient::~ic_opcdaclient(){

    size_t i;
    char buff[512]={0};
    memset(buff,0, sizeof(buff));

    EnterCriticalSection(&m_lock);
    sprintf(buff, "there %d server  %d group  %d item", m_servers.size(), m_groups.size(), m_items.size());
    for(i=0; i<m_servers.size(); ++i) {
        opc_server* opc = m_servers[i];
        delete opc;
    }
    for(i=0; i<m_groups.size(); ++i) {
        opc_group* group = m_groups[i];
        delete group;
    }
    LeaveCriticalSection(&m_lock);

    MessageBoxA(NULL,buff,NULL, MB_OK);

    DeleteCriticalSection(&m_lock);
 
}

int ic_opcdaclient::connect(const char* host, const char* name) {
    HANDLE hserver = NULL;
    int server = 0;

    
    size_t i;
    opc_server* opc = new opc_server;
    sprintf(opc->host, host, strlen(host));
    sprintf(opc->server, name, strlen(name));

    // find existing
    bool is_existing=false;
    EnterCriticalSection(&m_lock);
    for(i=0; i<m_servers.size(); ++i) {
        opc_server* opc = m_servers[i];
        if(strcmp(host, opc->host) ==0 &&
            strcmp(name, opc->server) ==0) {
            // server exists
            is_existing = true;
            break;
            
        }
    }
    if(is_existing) {
        LeaveCriticalSection(&m_lock);
        delete opc;
        return i;
    } else {
        // insert servers
        m_servers.push_back(opc);
        server = m_servers.size() - 1;
        LeaveCriticalSection(&m_lock);
    }

    hserver= ConnectOPC(host, name, TRUE);
    opc->opc = hserver;
    
    add_group(OPC_GROUP0_NAME, 1000, server);

 
    opc->thunk->bind(*this, &ic_opcdaclient::update_proc);
    opc_update_proc proc;
    proc = reinterpret_cast<opc_update_proc>(opc->thunk->code());;
    EnableOPCNotificationEx(hserver, proc);

    return server;
}

int ic_opcdaclient::add_group(const char* name, unsigned long rate, float dead_band, int server){

    int ret = 0;
    opc_server* opc=NULL;
    size_t i;

    // get server handle
    EnterCriticalSection(&m_lock);
    if(server < 0 || server >= m_servers.size()) {
        ret = -3;
    } else {
        opc = m_servers[server];
    }
    LeaveCriticalSection(&m_lock);

    if(!opc)
        return ret;

    // find existing
    bool is_existing=false;
    EnterCriticalSection(&m_lock);
    for(i=0; i<m_groups.size(); ++i) {
        opc_group* group = m_groups[i];
        if(strcmp(group->name, name) == 0 && group->rate == rate) {
            ret = i;
            is_existing = true;
            break;
        }
    }
    LeaveCriticalSection(&m_lock);
    
    if(is_existing)
        return i;
    

    HANDLE group = AddOPCGroup(opc->opc, name, &rate, &dead_band);
    if(group) {
        opc_group* grp = new opc_group;
        memset(grp, 0, sizeof(opc_group));
        grp->group = group;
        sprintf(grp->name,name);
        grp->opc = opc->opc;
        grp->rate = rate;
        grp->dead_band = dead_band;

        // ²åÈë ×é
        EnterCriticalSection(&m_lock);
        m_groups.push_back(grp);
        ret = m_groups.size() -1;
        LeaveCriticalSection(&m_lock);

    } else {
        ret = -2;
    }

    return ret;
}

int ic_opcdaclient::add_item(const char* name, int grp, int server) {
    opc_item item;
    opc_group *pg = NULL;
    int ret = 0;
    size_t i;

    opc_server* opc=NULL;

    EnterCriticalSection(&m_lock);
    if(server < 0 || server >= m_servers.size()) {
        ret = -3;
    } else {
        opc = m_servers[server];
    }

    if(grp < 0 || grp >= m_groups.size()) {
        ret = -2;
    }else {
        pg = m_groups[grp];
    }
    LeaveCriticalSection(&m_lock);

    if(!opc || !pg)
        return ret;

    // find existing
    bool is_existing = false;
    EnterCriticalSection(&m_lock);
    for(i=0; i< m_items.size(); ++i) {
        opc_item& item = m_items[i];
        if(item.group == pg->group && item.opc == opc->opc && strcmp(item.name, name) == 0) {
            ret = i;
            is_existing = true;
            break;
        }
    }
    LeaveCriticalSection(&m_lock);

    if(is_existing) return ret;

    memset(item.name, 0, sizeof(item.name));
    
    item.item = AddOPCItem(opc->opc, pg->group, name);
    if(!item.item) {
        return -2;
    }

    sprintf(item.name, name);
    item.group = pg->group;
    item.opc = pg->opc;

    // ²åÈë
    EnterCriticalSection(&m_lock);
    m_items.push_back(item);
    ret = m_items.size() - 1;
    LeaveCriticalSection(&m_lock);
    return ret;
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
    //MessageBoxA(NULL, buf, NULL, MB_OK);

    size_t i;

    EnterCriticalSection(&m_lock);
    for(i=0; i< m_items.size(); ++i) {
        opc_item& item = m_items[i];
        if(item.item == hItem && item.group == hGroup) {
            // update item
            item.value = var;
            item.quality = quality;
            item.timestamp = timestamp;

        }
    }
    LeaveCriticalSection(&m_lock);
}

const std::vector<opc_item>* ic_opcdaclient::items() const{
    return &m_items;
}
