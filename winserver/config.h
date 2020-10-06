#ifndef CONFIG_H_
#define CONFIG_H_

#include <vector>

typedef char* rpc_string;

class config_item{
public:
    config_item();
    int active;
    char item[64];
    char type[64];
    VARIANT value;
};

class config_group {
public:
    config_group();
    int active;
    char group[64];
    DWORD rate;
    float dead_band;
    std::vector<config_item> items;
};

class config_conn {
public:
    config_conn();
    int active;
    char host[64];
    char server[64];
    std::vector<config_group> groups;
};
class config_file {
public:
    config_file();
    int load_file(const char* name);
    int load(const char* s);
    int dump(rpc_string* s);
    int merge(const config_file* cfg);
    int diff(const config_file* cfg);
    char version[32];
    std::vector<config_conn> connections;
};
#endif //CONFIG_H_
