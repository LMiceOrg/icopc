#ifndef CONFIG_H_
#define CONFIG_H_

#include <vector>

typedef char* rpc_string;

class config_item{
public:
    config_item();
    int active;
    wchar_t item[64];
    wchar_t type[64];
    VARIANT value;
};

class config_group {
public:
    config_group();
    int active;
    wchar_t group[64];
    DWORD rate;
    float dead_band;
    std::vector<config_item> items;
};

class config_conn {
public:
    config_conn();
    int active;
    wchar_t host[64];
    wchar_t server[64];
    std::vector<config_group> groups;
};
class config_file {
public:
    config_file();
    int load_file(const rpc_string name);
    int load(const rpc_string s);
    int dump(rpc_string* s);
    int merge(const config_file* cfg);
    int diff(const config_file* cfg);
    wchar_t version[32];
    std::vector<config_conn> connections;
};
#endif //CONFIG_H_
