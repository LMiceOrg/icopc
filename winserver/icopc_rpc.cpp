#include "stdafx.h"

#include <stdlib.h>
#include <string.h>

#pragma warning(disable: 4503)
#include <map>
#include <vector>

#include "config.h"
#include "icopc_rpc.h"
#include "icopc_server.h"

#include "../libs/jansson/src/jansson.h"
#ifdef _WIN32
#ifdef _DEBUG
#pragma comment(lib, "../libs/jansson/src/jansson-mtd.lib")
#else
#pragma comment(lib, "../libs/jansson/src/jansson-mt.lib")
#endif
#endif



class rpc_state {
public:
  unsigned int m_ip;      /**< IP (host endian) */
  unsigned int m_nip;     /**< IP(net endian) */
  unsigned short m_port;  /**< port (host endian) */
  unsigned short m_nport; /**< port(net endian) */
};

class rpc_connect {
public:
  rpc_connect();
  ~rpc_connect();

  int _cdecl rpc_accept_proc(const struct sockaddr_in *, ws_data_callback *,
                             void **u, ws_user_close *, ws_timeout_callback *);
  int _cdecl rpc_data_proc(ws_io_context *data, void *u, char **buf, int *size);
  void _cdecl rpc_user_close(void *u);

  void _cdecl rpc_timeout_proc(void *);

  CRITICAL_SECTION lock; /**< 同步锁 */

  char m_version[64]; /**< 当前版本号 */
  char m_key[64];     /**< SM4 加密key */
  int m_encrypt;      /**< 是否加密 */
  int m_timeout;      /**< 通讯超时 */

  config_file m_config; /**< 配置文件 */

  std::vector<opc_server> m_servers;
  std::vector<opc_group> m_groups;
  std::vector<opc_item> m_items;

  opcdaclient *m_opc;

  call_thunk::unsafe_thunk *rpc_accept_proc_thunk;
  call_thunk::unsafe_thunk *rpc_data_proc_thunk;
  call_thunk::unsafe_thunk *rpc_user_close_thunk;
  call_thunk::unsafe_thunk *rpc_timeout_proc_thunk;

  std::vector<rpc_state *> m_connections; /**< 当前连接 */
};

static __inline int gen_rpc_return(char *buf, const char *method, int id,
                                   const char *key, const char *value);

rpc_connect::rpc_connect() {
  InitializeCriticalSection(&lock);

  rpc_accept_proc_thunk = new call_thunk::unsafe_thunk(
      5, NULL, call_thunk::cc_cdecl, call_thunk::cc_cdecl);
  rpc_accept_proc_thunk->bind(*this, &rpc_connect::rpc_accept_proc);

  rpc_data_proc_thunk = new call_thunk::unsafe_thunk(
      4, NULL, call_thunk::cc_cdecl, call_thunk::cc_cdecl);
  rpc_data_proc_thunk->bind(*this, &rpc_connect::rpc_data_proc);

  rpc_user_close_thunk = new call_thunk::unsafe_thunk(
      1, NULL, call_thunk::cc_cdecl, call_thunk::cc_cdecl);
  rpc_user_close_thunk->bind(*this, &rpc_connect::rpc_user_close);

  rpc_timeout_proc_thunk = new call_thunk::unsafe_thunk(
      1, NULL, call_thunk::cc_cdecl, call_thunk::cc_cdecl);
  rpc_timeout_proc_thunk->bind(*this, &rpc_connect::rpc_timeout_proc);

  memset(m_version, 0, sizeof(m_version));
  memset(m_key, 0, sizeof(m_key));

  m_opc = new opcdaclient();
}

rpc_connect::~rpc_connect() {
  // size_t i;
  DeleteCriticalSection(&lock);

  delete rpc_accept_proc_thunk;
  delete rpc_data_proc_thunk;
  delete rpc_user_close_thunk;
  delete rpc_timeout_proc_thunk;

  delete m_opc;
}

/** accept回调函数 */
int rpc_connect::rpc_accept_proc(const struct sockaddr_in *remote,
                                 ws_data_callback *pdc, void **u,
                                 ws_user_close *puc, ws_timeout_callback *ptc) {

  rpc_state *st = new rpc_state;
  st->m_nip = remote->sin_addr.s_addr;
  st->m_nport = remote->sin_port;
  st->m_ip = ntohl(st->m_nip);
  st->m_port = ntohs(st->m_nport);

  m_connections.push_back(st);

  *u = reinterpret_cast<void *>(st);
  *pdc = reinterpret_cast<ws_data_callback>(rpc_data_proc_thunk->code());
  *puc = reinterpret_cast<ws_user_close>(rpc_user_close_thunk->code());
  *ptc = reinterpret_cast<ws_timeout_callback>(rpc_timeout_proc_thunk->code());

  return 0;
}

void rpc_connect::rpc_timeout_proc(void *ptr) {
  char *buf = NULL;
  int send_size;
  ws_client_data *client = (ws_client_data *)ptr;
  rpc_session ses = reinterpret_cast<rpc_session>(this);
  wspkt *pkt;
  char *data;
  int pos = 0;

    buf = (char *)winserver_malloc(65536);
    pkt = (wspkt *)buf;
    data = pkt->data;
    

    pos = sprintf(data,
                  "{"
                  "\"id\":0,"
                  "\"method\":\"get_opc_items\","
                  "\"error\":{"
                  "\"code\":0,"
                  "\"message\":\"OK\""
                  "},"
                  "\"returns\":{"
                  "\"opc_items\":");
    data = data + pos;

    get_opc_items(&ses, &data, &send_size);

    send_size += sprintf(data + send_size, "} }");

    send_size += pos;

    ws_send_packet(client, pkt, send_size);

  winserver_free(buf);
}
/*
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
    int ws_len;

    memset(str, 0, 256);
    ws_len = wcslen(ws);
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
*/

static __inline int gen_rpc_ok(char *buf, const char *method, int id) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":[],"
                      "\"error\":{"
                      "\"code\":0,"
                      "\"message\":\"OK\""
                      "} }";
  return sprintf(buf, cmd_1, id, method);
}

int gen_rpc_return(char *buf, const char *method, int id, const char *key,
                   const char *value) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":{"
                      "\"%s\":%s"
                      "},"
                      "\"error\":{"
                      "\"code\":0,"
                      "\"message\":\"OK\""
                      "} }";
  return sprintf(buf, cmd_1, id, method, key, value);
}

static __inline int gen_rpc_bad(char *buf, const char *method, int id) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":[],"
                      "\"error\":{"
                      "\"code\":1,"
                      "\"message\":\"Unknown method\""
                      "} }";
  return sprintf(buf, cmd_1, id, method);
}

static __inline int gen_rpc_error(char *buf, const char *method, int id) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":[],"
                      "\"error\":{"
                      "\"code\":1,"
                      "\"message\":\"Failed\""
                      "} }";
  return sprintf(buf, cmd_1, id, method);
}

int rpc_connect::rpc_data_proc(ws_io_context *ctx, void *u, char **buf,
                               int *size) {
  rpc_state *st = reinterpret_cast<rpc_state *>(u);
  rpc_session ses = reinterpret_cast<rpc_session>(this);

  json_t *value;
  json_t *args;
  const char *method;
  int ret = RPC_OK;
  int id = 0;
  json_t *root = NULL;
  json_error_t err;

  root = json_loads(ctx->data + ctx->payload_pos, 0, &err);
  if (!root) {
    printf("request is not json format\n");
    return (0);
  }

  /*id */
  value = json_object_get(root, "id");
  id = json_integer_value(value);

  /* method */
  value = json_object_get(root, "method");
  method = json_string_value(value);
  if (!method) {
    printf("request no method\n");
    return (0);
  }

  /* parse method */
  if (strcmp(method, "set_rpc_version") == 0) {

    const char *version = NULL;
    args = json_object_get(root, "args");
    value = json_object_get(args, "version");
    version = json_string_value(value);

    ret = set_rpc_version(&ses, version);

    printf("set version %s\n", version);
    if (ret == RPC_OK) {
      *size = gen_rpc_ok(*buf, method, id);
    } else {
      *size = gen_rpc_error(*buf, method, id);
    }

  } else if (strcmp(method, "load_config") == 0) {
    const char *name = NULL;
    args = json_object_get(root, "args");
    value = json_object_get(args, "config");
    name = json_string_value(value);

    m_config.load_file(name);
    m_opc->apply_config(m_config);
    *size = 0;

  } else if (strcmp(method, "get_config") == 0) {
    char *config = NULL;
    m_config.dump(&config);

    *buf = (char *)winserver_malloc(strlen(config) + 1024);
    wspkt *pkt = (wspkt *)*buf;
    char *data = pkt->data;
    *size = gen_rpc_return(data, method, id, "config", config);

    free(config);

  } else if (strcmp(method, "get_opc_items") == 0) {

    *buf = (char *)winserver_malloc(65536);
    wspkt *pkt = (wspkt *)*buf;
    char *data = pkt->data;
    int pos = 0;

    pos = sprintf(data,
                  "{"
                  "\"id\":%d,"
                  "\"method\":\"%s\","
                  "\"error\":{"
                  "\"code\":0,"
                  "\"message\":\"OK\""
                  "},"
                  "\"returns\":{"
                  "\"opc_items\":",
                  id, method);
    data = data + pos;

    get_opc_items(&ses, &data, size);

    *size = sprintf(data + *size, "} }");

    *size += pos;

  } else if (strcmp(method, "quit") == 0) {
    ret = 1;
  }

  json_decref(root);

  return ret;
}

void rpc_connect::rpc_user_close(void *u) {
  rpc_state *st = reinterpret_cast<rpc_state *>(u);
  for (size_t i = 0; i < m_connections.size(); ++i) {
    if (m_connections[i] == st) {
      m_connections.erase(m_connections.begin() + i);
      break;
    }
  }

  delete st;
}

/* RPC 通讯设置 */

int set_rpc_version(rpc_session *ses, const char *version) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  if (strcmp(version, "1.0") != 0) {
    ret = RPC_NOTSUPPORT_VERSION;
  } else {
    memcpy(rpc->m_version, version, strlen(version));
  }

  return ret;
}

int set_rpc_encrypt(rpc_session *ses, const rpc_string key, int enable) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  if (enable) {
    memcpy(rpc->m_key, key, strlen(key));
  }
  rpc->m_encrypt = enable;

  return ret;
}

int set_rpc_timeout(rpc_session *ses, int timeout) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  rpc->m_timeout = timeout;

  return ret;
}

int get_rpc_version(rpc_session *ses, rpc_string version) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  memcpy(version, rpc->m_version, strlen(version));

  return ret;
}

/* 2 配置API */

int append_config_item(rpc_session *ses, const rpc_string config, int apply) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  config_file cfg;

  // 构造config
  ret = cfg.load(config);
  if (apply == 0)
    return ret;

  //合并config
  ret = rpc->m_config.merge(&cfg);
  return ret;
}

int remove_config_item(rpc_session *ses, const rpc_string config, int apply) {

  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  config_file cfg;

  // 构造config
  ret = cfg.load(config);
  if (apply == 0)
    return ret;

  // 移除config
  ret = rpc->m_config.diff(&cfg);
  return ret;
}

int get_config(rpc_session *ses, rpc_string *config) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  *config = NULL;
  rpc->m_config.dump(config);

  return ret;
}

int save_config_file(rpc_session *ses) {
  int ret = RPC_OK;
  rpc_string config;
  ret = get_config(ses, &config);

  // 保存文件
  return ret;
}

/* 3 服务控制API */

int stop_server_opc(rpc_session *ses) {
  int ret = RPC_OK;
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(ses);

  return ret;
}

int restart_server_opc(rpc_session *ses) {
  int ret = RPC_OK;

  ret = stop_server_opc(ses);

  // 启动OPC 数据服务
  return ret;
}

int stop_server(rpc_session *ses) {

  // system 执行 shutdown /s

  return RPC_OK;
}

int restart_server(rpc_session *ses) {
  // system 执行 shutdown /r

  return RPC_OK;
}

/* 4 数据访问 API */

int get_opc_items(rpc_session *ses, char **buf, int *size) {
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  opcdaclient *m_opc = rpc->m_opc;
  typedef std::map<const opc_group *, std::vector<const opc_item *> > svr_val;
  typedef opc_server *svr_key;
  std::map<svr_key, svr_val> svrs;
  const std::vector<opc_item> *items;

  m_opc->lock();
  items = m_opc->items();

  for (std::vector<opc_item>::const_iterator item_ite = items->begin(); item_ite != items->end(); ++item_ite) {
    const opc_item &item = *item_ite;
    svr_key key = item.oserver;
    const opc_group *grp = item.ogroup;
    svrs[key][grp].push_back(&item);
  }

  /* generate item json array */
  *size = 0;
  *size += sprintf(*buf + *size, "[\n");

  std::map<svr_key, svr_val>::iterator ite;
  for (ite = svrs.begin(); ite != svrs.end(); ++ite) {
    const svr_key &key = ite->first;
    const svr_val &svr_vals = ite->second;
    svr_val::const_iterator val_ite;

    /* conn */
    if (ite != svrs.begin()) {
      *size += sprintf(*buf + *size,
                       ","
                       "{\"host\":\"%s\", \"server\":\"%s\", "
                       "groups:[",
                       key->host, key->server);
    } else {
      *size += sprintf(*buf + *size,
                       "{\"host\":\"%s\", \"server\":\"%s\", "
                       "groups:[",
                       key->host, key->server);
    }

    for (val_ite = svr_vals.begin(); val_ite != svr_vals.end(); ++val_ite) {
      const opc_group *grp = val_ite->first;
      const std::vector<const opc_item *> &vals = val_ite->second;
      size_t item_pos;

      /* group */
      if (val_ite != svr_vals.begin()) {
        *size += sprintf(*buf + *size,
                         ","
                         "\"group\":\"%s\", \"items\":[",
                         grp->name);
      } else {
        *size +=
            sprintf(*buf + *size, "\"group\":\"%s\", \"items\":[", grp->name);
      }

      for (item_pos = 0; item_pos < vals.size(); ++item_pos) {
        const opc_item &item = *vals.at(item_pos);

        /* item */
        if (item_pos > 0) {
          *size += sprintf(*buf + *size, ",");
        }

        switch (item.value.vt) {
        case VT_EMPTY:
        case VT_NULL:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":null, \"type\":\"VT_NULL\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.quality, item.timestamp.dwLowDateTime,
              item.timestamp.dwHighDateTime);
          break;
        case VT_BOOL:
          if (item.value.boolVal) {
            *size += sprintf(
                *buf + *size,
                "{\"item\":\"%s\", \"value\":true, \"type\":\"VT_BOOL\", "
                "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
                item.name, item.quality, item.timestamp.dwLowDateTime,
                item.timestamp.dwHighDateTime);
          } else {
            *size += sprintf(
                *buf + *size,
                "{\"item\":\"%s\", \"value\":false, \"type\":\"VT_BOOL\", "
                "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
                item.name, item.quality, item.timestamp.dwLowDateTime,
                item.timestamp.dwHighDateTime);
          }
          break;
        case VT_UI1:
        case VT_I1:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_I1\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.bVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          break;
        case VT_UI2:
        case VT_I2:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_I2\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.iVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          break;
        case VT_UI4:
        case VT_I4:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_I4\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.lVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          break;
        case VT_INT:
        case VT_UINT:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_INT\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.intVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          break;
        case VT_R4:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%f, \"type\":\"VT_R4\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.fltVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          // sprintf(buf+sz, "VT_R4 %f", var.fltVal);
          break;
        case VT_R8:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%lf, \"type\":\"VT_R8\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.dblVal, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);
          // sprintf(buf+sz, "VT_R8 %lf", var.dblVal);
          break;
        case VT_BSTR:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":null, \"type\":\"VT_BSTR\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.quality, item.timestamp.dwLowDateTime,
              item.timestamp.dwHighDateTime);
          // sprintf(buf+sz, "VT_BSTR"); //, var.bstrVal
          break;
        case VT_ERROR:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_ERROR\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.value.scode, item.quality,
              item.timestamp.dwLowDateTime, item.timestamp.dwHighDateTime);

          break;
        case VT_DISPATCH:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_DISPATCH\"}, "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.quality, item.timestamp.dwLowDateTime,
              item.timestamp.dwHighDateTime);
          // sprintf(buf+sz, "VT_DISPATCH"); //var.pdispVal;
          break;
        case VT_UNKNOWN:
          *size += sprintf(
              *buf + *size,
              "{\"item\":\"%s\", \"value\":%d, \"type\":\"VT_UNKNOWN\", "
              "\"quality\":%d, \"timestamp\":{\"low\":%u, \"high\":%u} }\n",
              item.name, item.quality, item.timestamp.dwLowDateTime,
              item.timestamp.dwHighDateTime);
          // sprintf(buf+sz, "VT_UNKNOWN"); //var.punkVal;
          break;
          // default:
          //    break;
        }
        *size += sprintf(*buf + *size, "]\n");
      }
      *size += sprintf(*buf + *size, "]\n");
    }
    *size += sprintf(*buf + *size, "]\n");
  }

  m_opc->unlock();

  return RPC_OK;
}

int get_opc_item(rpc_session *ses, const rpc_string host,
                 const rpc_string server, const rpc_string group,
                 rpc_string name, rpc_item *item) {

  return RPC_OK;
}

int set_opc_item(rpc_session *ses, const rpc_string host,
                 const rpc_string server, const rpc_string group,
                 rpc_string name, VARIANT value) {

  return RPC_OK;
}

rpc_session create_rpc_session() {
  rpc_connect *rpc;
  rpc = new rpc_connect();
  return rpc;
}

void close_rpc_session(rpc_session *ses) {
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  delete rpc;
  *ses = NULL;
}

ws_accept_callback get_accept_callback(rpc_session *ses) {
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  return reinterpret_cast<ws_accept_callback>(
      rpc->rpc_accept_proc_thunk->code());
}

ws_timeout_callback get_timeout_callback(rpc_session *ses) {
  rpc_connect *rpc = reinterpret_cast<rpc_connect *>(*ses);
  return reinterpret_cast<ws_timeout_callback>(
      rpc->rpc_timeout_proc_thunk->code());
}
