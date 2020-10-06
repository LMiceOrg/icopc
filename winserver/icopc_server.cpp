#include "stdafx.h"

#include "icopc_server.h"

#define OPC_GROUP0_NAME "IC_OPCDACLIENT group0"

opc_item::opc_item() {
    memset(this, 0, sizeof(opc_item));
}

opc_group::opc_group() {
    memset(this, 0, sizeof(opc_group) );
}

/** 初始化 OPC server */
opc_server::opc_server() {
    memset(this, 0, sizeof(opc_server));
    thunk = new call_thunk::unsafe_thunk(6, NULL, call_thunk::cc_stdcall, call_thunk::cc_stdcall);
    printf("ctor thunk %p\n", thunk);
}

/** 析构 OPC server */
opc_server::~opc_server() {
    
    if(opc)
        DisconnectOPC(opc);
    opc = NULL;
    printf("dtor thunk %p\n", thunk);
    if(thunk)
        delete thunk;
    thunk=NULL;

}

bool opc_server::operator < (const opc_server* s2) const {
    return opc < s2->opc;
}

bool opc_server::operator ==(const opc_server* s2) const {
    return opc == s2->opc;
}

/** 初始化 OPC client */
opcdaclient::opcdaclient() {
    InitializeCriticalSection(&m_lock);

    CString cs;
    cs.GetBuffer(512);
    GetModuleFileName(NULL, cs.GetBuffer(512), 512);
    PathRemoveFileSpec(cs.GetBuffer(512));
    PathAppend(cs.GetBuffer(512), L"config.json");
    cs.ReleaseBuffer();
    FILE* fp;
    long size;
    fp = _wfopen(cs, L"rb");
    if(!fp) {
         return;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp)+1;
    fseek(fp, 0, SEEK_SET);

    char *s;
    s=(rpc_string)malloc(size);
    memset(s, 0, size);
    fread(s, 1, size, fp);
    fclose(fp);

    int ret;
    config_file cf;
    ret = cf.load(s);
    free(s);

    apply_config(cf);

}

/** 析构 OPC client */
opcdaclient::~opcdaclient() {

    lock();
    int i;
    for(i= m_servers.size(); i>0; --i) {
        remove_conn(i-1);
    }

    

    unlock();

    DeleteCriticalSection(&m_lock);
}

void opcdaclient::lock() {
    EnterCriticalSection(&m_lock);
}

void opcdaclient::unlock() {
    LeaveCriticalSection(&m_lock);
}



void opcdaclient::remove_conn(int i_server) {
    int i;
    HANDLE opc = m_servers[i_server]->opc;

    /* remove item */
    for(i = m_items.size(); i > 0; --i) {
        opc_item& the_item = m_items[i-1];
        if(the_item.opc == opc) {
            RemoveOPCItem(the_item.opc, the_item.group, the_item.item);
            m_items.erase(m_items.begin() + i-1);
            --i;
            if(i == 0) break;
        }
    }

    /* remove group */
    for(i = m_groups.size(); i > 0; -- i) {
        opc_group& the_group = m_groups[i-1];
        if(the_group.opc == opc) {
            RemoveOPCGroup(the_group.opc, the_group.group);
            m_groups.erase(m_groups.begin() + i-1);
            --i;
            if(i == 0) break;
        }
    }

    /* remove server */
    delete m_servers[i_server];
    m_servers.erase(m_servers.begin()+ i_server);

}

void opcdaclient::remove_group(int i_group) {
    opc_group& the_group = m_groups[i_group];
    size_t i;
    /* remove item */
    for(i = m_items.size(); i > 0; --i) {
        opc_item& the_item = m_items[i-1];
        if(the_item.group == the_group.group &&
                the_item.opc == the_group.opc) {
            RemoveOPCItem(the_item.opc, the_item.group, the_item.item);
            m_items.erase(m_items.begin() + i);
            --i;
            if(i==0) break;
        }
    }

    RemoveOPCGroup(the_group.opc, the_group.group);
    m_groups.erase(m_groups.begin() + i_group);

}

void opcdaclient::remove_item(int i_item) {
    opc_item& the_item = m_items[i_item];
    RemoveOPCItem(the_item.opc, the_item.group, the_item.item);
    m_items.erase(m_items.begin() + i_item);
}


void opcdaclient::apply_config(const config_file& cfg) {
    size_t i_server;
    size_t c_server;
    bool is_existing = true;
    const std::vector<config_conn>& conns = cfg.connections;

    lock();

    /* loop config server */
    for(c_server = 0; c_server < conns.size(); ++ c_server) {
        const config_conn& conn = conns[c_server];
        const std::vector<config_group>& groups = conn.groups;
        size_t c_group;
        int i_conn;
        HANDLE h_conn;

        if(conn.active != 1) {
            is_existing = false;
            for(i_server =0; i_server < m_servers.size(); ++i_server) {
                if(strcmp(m_servers[i_server]->host, conn.host) == 0 &&
                        strcmp(m_servers[i_server]->server, conn.server) == 0 ) {
                    is_existing = true;
                    break;
                }
            }
            if(is_existing) {
                remove_conn(i_server);
            }
            continue;
        }

        i_conn = add_connect(conn.host, conn.server);
        h_conn = m_servers[i_conn]->opc;



        /* loop config group */
        for(c_group = 0; c_group < groups.size(); ++c_group) {
            const config_group& group = groups[c_group];
            const std::vector<config_item>& items = group.items;
            size_t c_item;
            int i_group;
            HANDLE h_group;

            if(group.active != 1) {
                is_existing = false;
                for(i_group =0; i_group < m_groups.size(); ++i_group) {
                    if(strcmp(m_groups[i_group].name, group.group) == 0 ) {
                        is_existing = true;
                        break;
                    }
                }
                if(is_existing) {
                    remove_group(i_group);
                }
                continue;
            }

            i_group = add_group(group.group, group.rate, group.dead_band, i_conn);
            h_group = m_groups[i_group].group;



            /* loop config item */
            for(c_item = 0; c_item < items.size(); ++c_item) {
                const config_item& item = items[c_item];
                int i_item;

                if(item.active != 1) {
                    is_existing = false;
                    for(i_item =0; i_item < m_items.size(); ++i_item) {
                        if(strcmp(m_items[i_item].name, item.item) == 0 ) {
                            is_existing = true;
                            break;
                        }
                    }
                    if(is_existing) {
                        remove_item(i_item);
                    }
                    continue;
                }

                add_item(item.item, i_group, i_conn);
            }
        }
    }

    /* loop existing server */
    for(i_server =0; i_server < m_servers.size(); ++ i_server) {
        opc_server& the_server = *m_servers[i_server];
        size_t c_server;

        is_existing = false;
        /* loop config server */
        for(c_server = 0; c_server < conns.size(); ++ c_server) {
            const config_conn& conn = conns[c_server];
            if(strcmp(conn.host, the_server.host) == 0 &&
                    strcmp(conn.server, the_server.server) == 0) {
                if(conn.active == 1) {
                    is_existing = true;
                    break;
                }

            }
        }
        if(is_existing != true) {
            remove_conn(i_server);
            --i_server;
            if(i_server == 0) break;
            continue;
        }

        size_t i_group;
        size_t c_group;
        const std::vector<config_group>& groups = conns[c_server].groups;
        /* loop existing group */
        for(i_group = m_groups.size(); i_group > 0; --i_group) {
            opc_group& the_group = m_groups[i_group-1];

            if(the_group.opc != the_server.opc)
                continue;

            is_existing = false;
            /* loop config group */
            for(c_group = 0; c_group < groups.size(); ++ c_group) {
                const config_group& group = groups[c_group];
                if(strcmp(the_group.name, group.group) == 0) {
                    if(group.active == 1) {
                        is_existing = true;
                        break;
                    }
                }
            }

            if(is_existing != true) {
                remove_group(i_group-1);
                --i_group;
                if(i_group == 0)break;
                continue;
            }

            size_t i_item;
            size_t c_item;
            const std::vector<config_item>& items = groups[c_group].items;
            /* loop existing item */
            for(i_item = m_items.size(); i_item > 0; --i_item) {
                opc_item& the_item = m_items[i_item-1];

                if(the_item.opc != the_group.opc
                        || the_item.group != the_group.group) {
                    continue;
                }

                is_existing = false;
                /* loop config item */
                for(c_item = 0; c_item < items.size(); ++ c_item) {
                    const config_item &item = items[c_item];
                    if(strcmp(the_item.name, item.item) == 0) {
                        if(item.active == 1) {
                            is_existing = true;
                            break;
                        }
                    }
                }

                if(is_existing != true) {
                    remove_item(i_item-1);
                    --i_item;
                    if(i_item == 0) break;
                    continue;
                }
            } // for: i_item

        } // for: i_group
    }// for:i_server


    unlock();

}

const std::vector<opc_item>* opcdaclient::items() const {
    return &m_items;
}


int opcdaclient::add_connect(const char* host, const char* name) {
    HANDLE hserver = NULL;
    int server = 0;
    size_t i;
    bool is_existing=false;
    opc_server* opc;

    

    // find existing
    lock();
    for(i=0; i<m_servers.size(); ++i) {
        opc_server& the_opc = *m_servers[i];
        if(strcmp(host, the_opc.host) ==0 &&
            strcmp(name, the_opc.server) ==0) {
            // server exists
            is_existing = true;
            break;

        }
    }
    unlock();

    if(is_existing) {
        return i;
    }

    opc = new opc_server();
    sprintf(opc->host, host, strlen(host));
    sprintf(opc->server, name, strlen(name));
    opc->thunk->bind(*this, &opcdaclient::update_proc);

    /* connect opc server */
    hserver= ConnectOPC(host, name, TRUE);
    if(hserver == INVALID_HANDLE_VALUE) {
        /* failed */
        return -1;
    }
    opc->opc = hserver;

    /* notify thunk proc */
    EnableOPCNotificationEx(hserver, reinterpret_cast<opcda_update_proc>(opc->thunk->code()));

    /* add default group */
    lock();
    m_servers.push_back(opc);
    server = m_servers.size() - 1;
    unlock();
    add_group(OPC_GROUP0_NAME, 1000, server);

    return server;
}

int opcdaclient::add_group(const char* name, unsigned long rate, float dead_band, int server){

    int ret = 0;
    opc_server* opc=NULL;
    size_t i;
    bool is_existing=false;

    // get server handle
    lock();
    if(server < 0 || server >= m_servers.size()) {
        ret = -3;
    } else {
        opc = m_servers[server];
    }
    unlock();

    if(!opc)
        return ret;

    // find existing
    lock();
    for(i=0; i<m_groups.size(); ++i) {
        const opc_group* group = &m_groups[i];
        if(strcmp(group->name, name) == 0 && group->rate == rate) {
            ret = i;
            is_existing = true;
            break;
        }
    }
    unlock();

    if(is_existing)
        return ret;


    HANDLE group = AddOPCGroup(opc->opc, name, &rate, &dead_band);
    if(group == INVALID_HANDLE_VALUE) {
        ret = -2;
    } else {
        opc_group grp;
        grp.group = group;
        grp.opc = opc->opc;
        sprintf(grp.name,name);
        grp.rate = rate;
        grp.dead_band = dead_band;

        // 插入 组
        lock();
        m_groups.push_back(grp);
        ret = m_groups.size() -1;
        unlock();

    }

    return ret;
}

int opcdaclient::add_item(const char* name, int grp, int server) {
    opc_item item;
    int ret = 0;
    size_t i;
    bool is_existing = false;

    opc_server *opc=NULL;
    opc_group *pg = NULL;

    // get server handle
    lock();

    if(grp < 0 || grp >= m_groups.size()) {
        ret = -2;
    }else {
        pg = &m_groups[grp];
        for(i=0; i<m_servers.size(); ++i) {
            const opc_server& the_opc = *m_servers[i];
            if(the_opc.opc == pg->opc) {
                opc = m_servers[i];
                break;
            }
        }

        if(opc == NULL) {
            if (server < 0 || server >= m_servers.size()) {
                ret = -3;
            } else {
                opc = m_servers[server];
            }
        }
    }
    unlock();

    if(ret != 0)
        return ret;

    // find existing
    lock();
    for(i=0; i< m_items.size(); ++i) {
        const opc_item& the_item = m_items[i];
        if(the_item.group == pg->group && the_item.opc == opc->opc && strcmp(the_item.name, name) == 0) {
            ret = i;
            is_existing = true;
            break;
        }
    }
    unlock();

    if(is_existing) return ret;

    memset(item.name, 0, sizeof(item.name));
    sprintf(item.name, name);
    item.group = pg->group;
    item.opc = pg->opc;
    item.ogroup = pg;
    item.oserver = opc;

    item.item = AddOPCItem(item.opc, item.group, item.name);
    if(item.item == INVALID_HANDLE_VALUE) {
        return -2;
    }

    // 插入
    lock();
    m_items.push_back(item);
    ret = m_items.size() - 1;
    unlock();
    return ret;
}


void CALLBACK
opcdaclient::update_proc(HANDLE hConnection, HANDLE hGroup,
                            HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality) {
    CComVariant var(*pVar);
#ifdef _DEBUG
    char buf[512];
    size_t sz;
    memset(buf, 0, sizeof(buf));
    GetOPCItemNameFromHandle(hConnection, hGroup, hItem, buf, sizeof(buf));
    sz = strlen(buf);
    sprintf(buf + sz, " type[%d] = ", pVar->vt);
    sz = strlen(buf);



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
#endif
    size_t i;

    lock();
    for(i=0; i< m_items.size(); ++i) {
        opc_item& item = m_items[i];
        if(item.item == hItem && item.group == hGroup) {
            // update item
            item.value = var;
            item.quality = quality;
            item.timestamp = timestamp;

        }
    }
    unlock();
}
