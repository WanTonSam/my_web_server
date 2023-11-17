#include "webserver.h"

using namespace std;

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd,
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQuesize):
    port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
    timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
        srcDir_ = getcwd(nullptr, 256); // 获取当前工作目录
        assert(srcDir_);
        strncat(srcDir_, "/resources/", 16);    //设置资源目录
        HttpConn::userCount = 0;                // 初始化Http连接的静态成员
        HttpConn::srcDir = srcDir_;
        SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);  // 初始化SQL连接池

        InitEventMode_(trigMode);               // 初始化事件模式
        if (!InitSocket_()){isClose_ = true;}   // 初始化套接字，失败则设置关闭标志

        if (openLog){
            Log::Instance()->init(logLevel, "./log", ".log", logQuesize);   // 日志系统初始化
            if (isClose_) {LOG_ERROR("==================== Server init error ==================");}
            else {
                LOG_INFO("========= Server init ==============");
                LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger ? "true" : "false");
                LOG_INFO("Listen Mode: %s, OpenConn MOde: %s", 
                                (listenEvent_ & EPOLLET ? "ET" : "LT"),
                                (connEvent_ & EPOLLET ? "ET" : "LT"));
                LOG_INFO("LogSys level: %d", logLevel);
                LOG_INFO("srcDir: %s", HttpConn::srcDir);
                LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum,  threadNum);
            }
        }
}

WebServer::~WebServer() {
    close(listenFd_);    // 关闭监听文件描述符
    isClose_ = true;
    free(srcDir_);       // 释放资源目录路径
    SqlConnPool::Instance()->ClosePool();   // 关闭SQL连接池
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_  = EPOLLRDHUP;             // 设置监听事件
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // 设置连接事件
    switch (trigMode)    // 根据传入的模式调整事件模式
    {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;      // 启用边缘触发模式
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_  |= EPOLLET;   // 监听事件和连接事件都使用边缘触发
            connEvent_ |= EPOLLET;
            break;
        default :
            listenEvent_  |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
    }

    HttpConn::isET = (connEvent_ & EPOLLET);    // 设置Http连接是否为边缘触发模式
}
void WebServer::Start() {       // 启动Web服务器
    int timeMS = -1;            // epoll wait超时时间，-1表示无限等待
    if (!isClose_) {LOG_INFO("============ Server start =============="); }
    while (!isClose_) {
        if (timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();     // 获取下一个定时事件的时间
        }
        int eventCnt = epoller_->Wait(timeMS);  // 等待事件
        for (int i = 0; i < eventCnt; i++) {    // 处理每一个事件
            int fd = epoller_->GetEventFd(i);   // 获取文件描述符
            uint32_t events = epoller_->GetEvents(i);   // 获取事件类型
            if (fd == listenFd_) {
                DealListen_();                  // 处理监听事件
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 处理异常事件
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if (events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if (events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {      // 关闭连接
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();             // 关闭客户端连接
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {      // 添加新的客户端
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if (timeoutMS_ > 0) {           // 如果设置了超时时间，则添加到定时器中
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);  // 将文件描述符添加到epoll中，并设置为非阻塞模式
    SetFdNonblock(fd);                          // 设置文件描述符为非阻塞
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {     // 处理监听事件
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if (fd <= 0) { return ;}
        else if (HttpConn::userCount >= MAX_FD) {   // 客户端数量超过最大值
            SendError_(fd, "Server busy!");
            LOG_WARN("Client is full!");
            return;
        }
        AddClient_(fd, addr);           // 添加新客户端
    } while (listenEvent_ & EPOLLET);   // 如果是边缘触发模式，需要循环处理
}

void WebServer::DealRead_(HttpConn* client) {       // 处理读事件
    assert(client);
    ExtenTime_(client);                             // 延长客户端超时时间
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));  // 将读取任务添加到线程池
}

void WebServer::DealWrite_(HttpConn* client) {      // 处理写事件
    assert(client);
    ExtenTime_(client);                             // 延长客户端超时时间
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));    // 将写入任务添加到线程池
}

void WebServer::ExtenTime_(HttpConn* client) {      // 延长客户端超时时间
    assert(client);
    if (timeoutMS_ > 0) {
        timer_->adjust(client->GetFd(), timeoutMS_);      // 如果设置了超时时间，则调整定时器
    }
}

void WebServer::OnRead_(HttpConn* client) {         // 处理读事件
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);      // 处理读取到的数据
}

void WebServer::OnProcess(HttpConn* client) {       // 处理客户端请求
    if(client->process()) {                         // 如果处理成功，准备写回数据
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    }
    else {      // 继续读取更多数据
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {        // 处理写事件
    assert(client);
    int ret = -1;
    int writeErrono = 0;
    ret = client->write(&writeErrono);              // 执行写操作
    if (client->ToWriteBytes() == 0) {              // 如果数据已经全部写入
        if (client->IsKeepAlive()) {                // 如果是长连接，继续处理请求
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0) {
        if (writeErrono == EAGAIN) {    //EAGAIN 是一个错误码，表示非阻塞操作无法立即完成,在这种情况下，意味着输出缓冲区已满，现在不能发送更多数据，稍后可以重试。
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!", port_);
        return false;
    }
    addr.sin_family = AF_INET;                  // 设置地址族
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 接受任何地址
    addr.sin_port = htons(port_);               // 设置端口号
    struct linger optLinger = { 0 };
    if (openLinger_) {                          // 设置优雅关闭选项
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);    // 创建套接字
    if (listenFd_ < 0) {
        LOG_ERROR("Creat socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));  // 设置linger选项
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));    // 设置socket选项SO_REUSEADDR，允许重用本地地址和端口
    if (ret == -1) {
        LOG_ERROR("set socket setsocketopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR("Listen Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);   // 将监听的文件描述符添加到epoll事件监听中
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }

    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {  // 设置文件描述符为非阻塞模式
    assert(fd > 0);
    return fcntl(fd, F_SETFL,fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}