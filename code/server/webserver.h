#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"
class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqpPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    ~WebServer();
    void Start();

private:

    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);

    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char* info);
    void ExtenTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static  int SetFdNonblock(int fd);

    int port_;               // 服务器端口
    bool openLinger_;       // 是否开启linger选项
    int timeoutMS_;         // 超时时间（毫秒）
    bool isClose_;          // 服务器是否关闭的标志
    int listenFd_;          // 监听文件描述符
    char* srcDir_;          // 资源目录
    uint32_t listenEvent_;  // 监听事件类型
    uint32_t connEvent_;    // 连接事件类型

    std::unique_ptr<HeapTimer> timer_;          // 定时器
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller>  epoller_;         // Epoller对象
    std::unordered_map<int, HttpConn> users_;   // 存储客户端连接的map
};

#endif