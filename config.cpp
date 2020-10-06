#include "stdafx.h"
#include "config.h"

#include "libs/jansson/src/jansson.h"

static inline void u2w(const char* us, int us_len, wchar_t* ws, int ws_len);

static inline void get_wstr(const json_t* value, wchar_t* ws, int ws_len);

static inline json_t* get_jstr(const wchar_t* ws);

config_file::config_file() {
    memset(version, 0, sizeof(version));
    connections.clear();
}

int config_file::load_file(const rpc_string name) {
    FILE* fp;
    long size;

    {
        const char* cdata=
      "{"
      "\"command\":\"test\""
      "}";
        json_t* root;
        json_error_t err;
        root = json_loads(cdata, 0, &err);
        if(!root) {
            MessageBox(NULL, L"load test failed", NULL, MB_OK);
        }
    }

    fp = fopen(name, "rb");
    if(!fp) {
         return -1;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp)+1;
    fseek(fp, 0, SEEK_SET);

    rpc_string s;
    s=(rpc_string)malloc(size);
    memset(s, 0, size);
    fread(s, 1, size, fp);
    fclose(fp);

    int ret;
    ret = load(s);
    free(s);
    return ret;

}

int config_file::load(const rpc_string s) {

    // 1. 清除缓存 
    connections.clear();
    memset(version, 0, 32*2);

    // 2. 解析JSON
    json_error_t err;
    json_t *root;
    json_t* value;
  
    root = json_loads(s, 0, &err);
    if(!root) {
        return -1;
    }

    
    value = json_object_get(root, "version");
    if(json_is_string(value)) {
        const char* us = json_string_value(value);
        int us_len = strlen(us);
        u2w(us, us_len, version, 32);
    }
    //MessageBox(NULL, version, NULL, MB_OK);

    json_t *conns;
    conns = json_object_get(root, "connections");
    if(json_is_array(conns)) {
        int i_conn;
        json_t *conn;
        json_array_foreach(conns, i_conn, conn) {
            config_conn cfg_conn;
            
            value = json_object_get(conn, "active");
            if(json_is_integer(value)) {
                cfg_conn.active = json_integer_value(value);
            }

            value = json_object_get(conn, "host");
            get_wstr(value, cfg_conn.host, 64);

            value = json_object_get(conn, "server");
            get_wstr(value, cfg_conn.server, 64);

            json_t* groups;
            groups = json_object_get(conn, "groups");
            if(json_is_array(groups)) {
                int i_group;
                json_t *group;
                json_array_foreach(groups, i_group, group) {
                    config_group cfg_group;

                    value = json_object_get(group, "active");
                    if(json_is_integer(value)) {
                        cfg_group.active = json_integer_value(value);
                    }

                    value = json_object_get(group, "group");
                    get_wstr(value, cfg_group.group, 64);

                    value = json_object_get(group, "rate");
                    if(json_is_integer(value)) {
                        cfg_group.rate = json_integer_value(value);
                    }

                    value = json_object_get(group, "dead_band");
                    if(json_is_number(value)) {
                        cfg_group.dead_band = json_number_value(value);
                    }

                    json_t *items;
                    items = json_object_get(group, "items");
                    if(json_is_array(items)) {
                        int i_item;
                        json_t *item;
                        
                        json_array_foreach(items, i_item, item) {
                            config_item cfg_item;
                            
                            value = json_object_get(item, "active");
                            if(json_is_integer(value)) {
                                cfg_item.active = json_integer_value(value);
                            }

                            value = json_object_get(item, "item");
                            get_wstr(value, cfg_item.item, 64);

                            value = json_object_get(item, "type");
                            get_wstr(value, cfg_item.type, 64);

                            cfg_group.items.push_back(cfg_item);
                        }

                    } // if items
                    cfg_conn.groups.push_back(cfg_group);
                }
            } // if groups
            connections.push_back(cfg_conn);
        }
    } // if conns

    json_decref(root);

    return 0;
}

int config_file::dump(rpc_string* s) {

    json_t *root = NULL;
    json_t *value = NULL;

    if(!s) {
        return -1;
    }

    root = json_object();
    

    value = get_jstr(version);
    json_object_set_new_nocheck(root, "version", value);

    json_t *conns;
    conns = json_array();
    json_object_set_new_nocheck(root, "connections", conns);

    size_t i_conn;
    for(i_conn=0; i_conn< connections.size(); ++i_conn) {
        const config_conn& cfg_conn = connections[i_conn];

        json_t *conn;
        conn = json_object();
        json_array_append_new(conns, conn);

        value = json_integer(cfg_conn.active);
        json_object_set_new_nocheck(conn, "active", value);
        
        value = get_jstr(cfg_conn.host);
        json_object_set_new_nocheck(conn, "host", value);

        value = get_jstr(cfg_conn.server);
        json_object_set_new_nocheck(conn, "server", value);

        json_t* groups;
        groups = json_array();
        json_object_set_new_nocheck(conn, "groups", groups);
        
        size_t i_group;
        for(i_group=0; i_group< cfg_conn.groups.size(); ++i_group) {

            const config_group& cfg_group = cfg_conn.groups[i_group];

            json_t *group;
            group = json_object();
            json_array_append_new(groups, group);

            value = json_integer(cfg_group.active);
            json_object_set_new_nocheck(group, "active", value);

            value = get_jstr(cfg_group.group);
            json_object_set_new_nocheck(group, "group", value);

            value = json_integer(cfg_group.rate);
            json_object_set_new_nocheck(group, "rate", value);

            value = json_real(cfg_group.dead_band);
            json_object_set_new_nocheck(group, "dead_band", value);

            json_t *items;            
            items = json_array();
            json_object_set_new_nocheck(group, "items", items);

            size_t i_item;
            for(i_item=0; i_item < cfg_group.items.size(); ++i_item) {
                const config_item& cfg_item = cfg_group.items[i_item];

                json_t *item;
                item = json_object();
                json_array_append_new(items, item);

                value = json_integer(cfg_item.active);
                json_object_set_new_nocheck(item, "active", value);

                value = get_jstr(cfg_item.item);
                json_object_set_new_nocheck(item, "item", value);

                value = get_jstr(cfg_item.type);
                json_object_set_new_nocheck(item, "type", value);

            }
        }
    }

    char* json = json_dumps(root, 4);
    json_decref(root);

    *s = json;

    return 0;
}

int config_file::merge(const config_file* cfg) {
    size_t i_conn;
    for(i_conn=0; i_conn < cfg->connections.size(); ++i_conn) {
        const config_conn& cfg_conn = cfg->connections[i_conn];
        
        size_t s_conn;
        bool find = false;
        for(s_conn=0; s_conn<connections.size(); ++s_conn) {
            const config_conn& self_conn = connections[s_conn];

            if( wcscmp(self_conn.host, cfg_conn.host) ==0 &&
                wcscmp(self_conn.server, cfg_conn.server) == 0) {
                // 同一个连接
                find = true;
                break;
            }
        }

        if(find) {
            // 合并现有连接
            config_conn& self_conn = connections[s_conn];
            size_t i_group;

            for(i_group=0; i_group<cfg_conn.groups.size(); ++i_group) {
                const config_group& cfg_group = cfg_conn.groups[i_group];

                size_t s_group;
                bool find_group = false;
                for(s_group=0; s_group< self_conn.groups.size(); ++s_group) {
                    const config_group &self_group = self_conn.groups[s_group];
                    if(wcscmp(self_group.group, cfg_group.group) == 0 ) {
                        find_group = true;
                        break;
                    }
                }

                if(find_group) {
                    // 合并现有 组
                    config_group& self_group = self_conn.groups[s_group];
                    size_t i_item;

                    for(i_item=0; i_item<cfg_group.items.size(); ++i_item) {
                        const config_item& cfg_item = cfg_group.items[i_item];

                        size_t s_item;
                        bool find_item = false;
                        for(s_item=0; s_item < self_group.items.size(); ++s_item) {
                            const config_item& self_item = self_group.items[s_item];
                            if(wcscmp(self_item.item, cfg_item.item) == 0) {
                                find_item = true;
                                break;
                            }
                        }

                        if(find_item) {
                            // 合并现有 ITEM
                                config_item& self_item = self_group.items[s_item];
                                self_item.active = cfg_item.active;
                                memcpy(self_item.type, cfg_item.type, sizeof(self_item.type));
                                memcpy(&self_item.value, &cfg_item.value, sizeof(self_item.value));
                        } else {
                            // 插入新 ITEM
                            self_group.items.push_back(cfg_item);
                        }
                    }
                } else {
                    // 插入新组
                    self_conn.groups.push_back(cfg_group);
                }

            }

        } else {
            // 插入新的连接
            connections.push_back(cfg_conn);
        }
    }

    // 删除空列表
    int s_conn;
    for(s_conn= connections.size() - 1; s_conn >= 0; --s_conn) {
        config_conn &cfg_conn = connections[s_conn];
        if(cfg_conn.groups.size() == 0) {
            connections.erase(connections.begin() + s_conn);
        } else {
            int s_group;
            for(s_group=cfg_conn.groups.size() - 1; s_group >= 0; --s_group) {
                config_group& cfg_group = cfg_conn.groups[s_group];
                if(cfg_group.items.size() == 0) {
                    cfg_conn.groups.erase(cfg_conn.groups.begin() + s_group);
                } 
            }
        }
    }

    return 0;
}

int config_file::diff(const config_file* cfg) {
    size_t i_conn;
    for(i_conn=0; i_conn < cfg->connections.size(); ++i_conn) {
        const config_conn& cfg_conn = cfg->connections[i_conn];

        size_t s_conn;
        bool find = false;
        for(s_conn=0; s_conn<connections.size(); ++s_conn) {
            const config_conn& self_conn = connections[s_conn];

            if( wcscmp(self_conn.host, cfg_conn.host) ==0 &&
                wcscmp(self_conn.server, cfg_conn.server) == 0) {
                // 同一个连接
                find = true;
                break;
            }
        }

        // 没有对应连接 则继续
        if(!find) {
            continue;
        }

        config_conn& self_conn = connections[s_conn];
        size_t i_group;

        for(i_group=0; i_group<cfg_conn.groups.size(); ++i_group) {
            const config_group& cfg_group = cfg_conn.groups[i_group];

            size_t s_group;
            bool find_group = false;
            for(s_group=0; s_group< self_conn.groups.size(); ++s_group) {
                const config_group &self_group = self_conn.groups[s_group];
                if(wcscmp(self_group.group, cfg_group.group) == 0 ) {
                    find_group = true;
                    break;
                }
            }

            // 没有对应组 则继续
            if (!find_group) {
                continue;
            }

            config_group &self_group = self_conn.groups[s_group];
            size_t i_item;

            for(i_item=0; i_item<cfg_group.items.size(); ++i_item) {
                const config_item& cfg_item = cfg_group.items[i_item];

                size_t s_item;
                bool find_item = false;
                for(s_item=0; s_item < self_group.items.size(); ++s_item) {
                    const config_item& self_item = self_group.items[s_item];
                    if(wcscmp(self_item.item, cfg_item.item) == 0) {
                        find_item = true;
                        break;
                    }
                }

                // 没有对应item 则继续
                if (!find_item) {
                    continue;
                }

                // 删除 ITEM
                self_group.items.erase(self_group.items.begin()+s_item);

            }// FOR I_ITEM

        } // FOR I_GROUP
    } // FOR I_CONN

    // 删除空列表
    int s_conn;
    if(connections.size() == 0) return 0;
    for(s_conn= connections.size()-1; s_conn >=0; --s_conn) {
        config_conn &cfg_conn = connections[s_conn];
        if(cfg_conn.groups.size() == 0) {
            connections.erase(connections.begin() + s_conn);
        } else {
            int s_group;
            for(s_group=cfg_conn.groups.size() -1; s_group>=0; --s_group) {
                config_group& cfg_group = cfg_conn.groups[s_group];
                if(cfg_group.items.size() == 0) {
                    cfg_conn.groups.erase(cfg_conn.groups.begin() + s_group);
                } 
            }
        }
    }

    return 0;
}


void u2w(const char* us, int us_len, wchar_t* ws, int ws_len) {
    int ret = MultiByteToWideChar(CP_UTF8, 0, us, us_len, ws, ws_len);
    ret;
}

void w2u(const wchar_t* ws, int ws_len, char* us, int us_len) {
    WideCharToMultiByte(CP_UTF8, 0, ws, ws_len, us, us_len, 0, 0);
}

json_t* get_jstr(const wchar_t* ws) {
    char str[256];
    json_t* value;
    memset(str, 0, 256);
    int ws_len = wcslen(ws);
    w2u(ws, ws_len, str, 256);
    value = json_string(str);

    return value;
}

void get_wstr(const json_t* value, wchar_t* ws, int ws_len) {
    if(json_is_string(value)) {
        const char* us = json_string_value(value);
        int us_len = strlen(us);
        u2w(us, us_len, ws, ws_len);
    }
}

config_item::config_item() {
    active=0;
    memset(item, 0, sizeof(item));
    memset(type, 0, sizeof(type));
}

config_group::config_group() {
    active = 0;
    memset(group, 0, sizeof(group));
    rate = 0;
    dead_band=0;
    items.clear();
}

config_conn::config_conn() {
    active = 0;
    memset(host, 0, sizeof(host));
    memset(server, 0, sizeof(server));
    groups.clear();
}