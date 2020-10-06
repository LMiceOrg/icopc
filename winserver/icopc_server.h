#ifndef ICOPC_SERVER_H
#define ICOPC_SERVER_H

/* opcda client server */

#include "config.h"

struct opc_group;
struct opc_server;

/** OPC item */
struct opc_item{
    HANDLE item;
    HANDLE group;
    HANDLE opc;

    char name[128];
    CComVariant value;
    DWORD quality;
    FILETIME timestamp;

    opc_group* ogroup;
    opc_server* oserver;

    explicit opc_item();
};

/** OPC group */
struct opc_group {
    HANDLE group;
    HANDLE opc;
    char name[128];
    unsigned long rate;
    float dead_band;

    explicit opc_group();
};

/** OPC server */
struct opc_server {
    char host[128];
    char server[128];
    HANDLE opc;
    call_thunk::unsafe_thunk* thunk;

    explicit opc_server();
    ~opc_server();

    bool operator < (const opc_server* s2) const;
    bool operator ==(const opc_server* s2) const;
};

typedef void  (CALLBACK * opcda_update_proc)(HANDLE, HANDLE,
        HANDLE, VARIANT *, FILETIME, DWORD);

class opcdaclient {
public:
    opcdaclient();
    ~opcdaclient();

    void lock();
    const std::vector<opc_item>* items() const;
    void unlock();

    void apply_config(const config_file& cfg);

private:
    int add_connect(const char* host, const char* name);
    int add_group(const char* name, unsigned long rate, float dead_band, int server=0);
    int add_item(const char* name, int group=0, int server=0);
    void CALLBACK update_proc(HANDLE hConnection, HANDLE hGroup,
        HANDLE hItem, VARIANT *pVar, FILETIME timestamp, DWORD quality);


    void remove_conn(int i_server);
    void remove_group(int i_group);
    void remove_item(int i_item);


private:
    std::vector<opc_server*> m_servers;
    std::vector<opc_group> m_groups;
    std::vector<opc_item>m_items;
    CRITICAL_SECTION  m_lock;
};

#endif // ICOPC_SERVER_H
