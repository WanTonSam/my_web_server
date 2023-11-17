#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>           // 引入系统类型定义
#include <sys/uio.h>             // 包含readv/writev函数的头文件
#include <arpa/inet.h>           // 提供sockaddr_in结构和网络函数
#include <stdlib.h>              // 包含atoi()等标准库函数
#include <errno.h>               // 包含错误号定义

#include "../log/log.h"          // 引入日志模块
#include "../pool/sqlconnRAII.h" // 引入SQL连接RAII封装
#include "../buffer/buffer.h"    // 引入缓冲区处理模块
#include "httprequest.h"         // 引入HTTP请求处理模块
#include "httpresponse.h"        // 引入HTTP响应处理模块

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);// 初始化函数，设置套接字和地址

    ssize_t read(int *saveErrno);               // 从套接字读取数据，保存错误码

    ssize_t write(int *saveErrno);              // 向套接字写入数据，保存错误码

    void Close();                               // 关闭连接

    int GetFd() const;                           // 获取文件描述符

    int GetPort() const;                        // 获取端口号

    const char* GetIP() const;                  // 获取IP地址

    sockaddr_in GetAddr() const;                // 获取地址结构

    bool process();                             // 处理读取的数据

    int ToWriteBytes() {                        // 返回待写入的字节数
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    bool IsKeepAlive() const {                  // 检查连接是否保持活动状态
        return request_.IsKeepAlive();
    }

    static bool isET;               // 静态成员变量，表示是否使用边缘触发模式     
    static const char* srcDir;      // 静态成员变量，表示资源目录
    static std::atomic<int> userCount; // 静态原子成员变量，追踪用户数

private:

    int fd_;                        // 文件描述符，表示网络连接
    struct sockaddr_in addr_;       // 网络地址结构

    bool isClose_;                   // 标记连接是否已关闭

    int iovCnt_;                     // 表示iovec结构数组的数量
    struct iovec iov_[2];            // iovec结构数组，用于readv/writev

    Buffer readBuff_;                // 读缓冲区
    Buffer writeBuff_;               // 写缓冲区

    HttpRequest request_;            // HTTP请求对象
    HttpResponse response_;          // HTTP响应对象
};

#endif  //HTTP_CONN_H