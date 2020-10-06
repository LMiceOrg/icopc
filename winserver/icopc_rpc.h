#ifndef ICOPC_RPC_H
#define ICOPC_RPC_H

#include "../websocket/wsserver.h"
/* 类型定义 */

/** RPC错误号 */
enum rpc_errors {
    RPC_SUCCESS=0,
    RPC_OK=RPC_SUCCESS,
    RPC_ERROR,
    RPC_FAILURE=RPC_ERROR,
    RPC_NOTSUPPORT_VERSION
};

/** RPC 会话 */
typedef void* rpc_session;

/** rpc_string64 类型
 * max length 64 bytes,
 * codepage: utf-8,
 * null terminated */
typedef char(*rpc_string64)[64];

/** rpc_string128 类型
 * max length 128 bytes,
 * codepage: utf-8,
 * null terminated */
typedef char(*rpc_string128)[128];

/** rpc_string 类型
 * variant length,
 * codepage: utf-8,
 * null terminated */
typedef char* rpc_string;

struct rpc_item {
    rpc_string128 name;
    VARIANT value;
    unsigned int quality;
    FILETIME timestamp;
};

#ifndef WS_IO_CONTEXT
#define WS_IO_CONTEXT

/** RPC请求数据类型
 * overlapped
 */
typedef struct _ws_io_context {
    WSAOVERLAPPED               overlapped;
    WSABUF                      wsabuf;
    int op;
    int data_len;
    int payload_len;
    int payload_pos;
    char data[65536];
} ws_io_context;
#endif


rpc_session create_rpc_session();

void close_rpc_session(rpc_session* ses);

ws_accept_callback get_accept_callback(rpc_session* ses);

ws_timeout_callback get_timeout_callback(rpc_session* ses);

/* 1 通讯设置API */

/** 1.1 设置版本号
      * Args:
      * [version]: version string
      * Returns:
      * Error:
      * [code]: =0 成功， =Other 失败
      */
int set_rpc_version(rpc_session* ses, const char* version);

/** 1.2 设置数据加密 (SM4)
      * Args:
      * [key]: 加密key
      * [enable]: 是否启用数据加密 =0 明文传输， =Other 加密传输
      * Return:
      * Error:
      * [code]: =0 成功, =Other 失败
      */
int set_rpc_encrypt(rpc_session* ses, const rpc_string key, int enable);

/** 1.3 设置访问超时
    * Args:
    * [timeout]: 超时时长 单位毫秒
    * Returns:
    * Error:
    * [code]: =0 成功, =Other 失败
    */
int set_rpc_timeout(rpc_session* ses, int timeout);

/** 1.4 获取版本号
 * Args:
 * Returns:
 * [version] 版本字符串
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int get_rpc_version(rpc_session* ses, rpc_string version);

/* 2 配置API */

/** 2.1 增加配置项
 * Args:
 * [config]: 配置对象 json string
 * [enable]: 是否生效 =0 仅检测， =1 应用
 * Returns:
 * [code]: =0 成功, =Other 失败
 */
int append_config_item(rpc_session* ses, const rpc_string config, int apply);

/** 2.2 移除配置项
 * Args:
 * [config]: 配置对象 json string
 * [enable]: 是否生效 =0 仅检测， =1 应用
 * Returns:
 * [code]: =0 成功, =Other 失败
 */
int remove_config_item(rpc_session* ses, const rpc_string config, int apply);

/** 2.3 获取当前配置
 * Args:
 * Returns:
 * [config] 配置json string
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int get_config(rpc_session* ses, rpc_string *config);

/** 2.3 获取配置文件
 * Args:
 * Returns:
 * Error:
 * [code] 配置json string
 */
int save_config_file(rpc_session* ses);

/* 3 服务控制API */

/** 3.1 停止OPC服务
 * Args:
 * Returns:
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int stop_server_opc(rpc_session* ses);

/** 3.2 重启OPC服务
 * Args:
 * Returns:
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int restart_server_opc(rpc_session* ses);

/** 3.3 关闭电脑
 * Args:
 * Returns:
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int stop_server(rpc_session* ses);

/** 3.4 重启电脑
 * Args:
 * Returns:
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int restart_server(rpc_session* ses);

/* 4 数据访问 API */

/** 4.1 读取所有配置项
 * Args:
 * Returns:
 * [items] 配置项数组 rpc_item
 * Error:
 * [code]: =0 成功, =Other 失败
 */
//int get_opc_items(rpc_session* ses, const rpc_string host, const rpc_string server, const rpc_string group, rpc_item** items);
int get_opc_items(rpc_session* ses, char** buf, int *size);
/** 4.2 读取配置项
 * Args:
 * [config]: 配置项名称
 * Returns:
 * [item]: 配置项 json string
 * Error:
 * [code]: =0 成功, =Other 失败
 */
int get_opc_item(rpc_session* ses, const rpc_string host, const rpc_string server, const rpc_string group, rpc_string128 name, rpc_item* item);

/** 4.3 设置配置项
 * Args:
 * [config]: 配置项名称, 含有配置项值
 * Returns:
 * Error:
 * [0]: =0 成功, =Other 失败
 */
int set_opc_item(rpc_session* ses, const rpc_string host, const rpc_string server, const rpc_string group, rpc_string128 name, VARIANT value);

/* 5 RPC-IOCP 回调函数 API */

/** 5.1 rpc请求处理
 * Args：
 * data : 请求数据 websocket-payload + json-string
 * buf : 反馈数据内容 json-string
 * size :  反馈数据长度
 * Returns:
 * Error:
 * [ret] : =0 正常处理 =Other 处理异常，断开当前连接
 */
int proc_rpc_request(ws_io_context * data, char ** buf, int* size);

#endif // ICOPC_RPC_H
