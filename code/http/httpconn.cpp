#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;       //静态成员初始化
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
}

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::init(int fd, const sockaddr_in& addr) {   // 初始化连接的函数
    assert(fd > 0);                                      // 断言文件描述符有效
    userCount++;                                         // 增加用户计数
    addr_ = addr;                                        // 设置地址
    fd_ = fd;                                            // 设置文件描述符
    writeBuff_.RetrieveAll();                            // 清空写缓冲区
    readBuff_.RetrieveAll();                             // 清空读缓冲区
    isClose_ = false;                                    // 标记连接为开启状态
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount); // 记录日志
}

void HttpConn::Close() {                                 // 关闭连接的函数
    response_.UnmapFile();                               // 如果有映射文件，先解除映射
    if (isClose_ == false) {                             // 如果连接未关闭
        isClose_ = true;                                 // 标记为已关闭
        userCount--;                                     // 减少用户计数
        close(fd_);                                      // 关闭文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount); // 记录日志
    }
}
int HttpConn::GetFd() const {
    return fd_;
}

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {        // 获取IP地址的函数
    return inet_ntoa(addr_.sin_addr);       // 将网络地址转换为字符串形式
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {                 // 从连接读取数据的函数
    ssize_t len = -1;                                    // 初始化读取长度为-1
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);          // 从文件描述符读取数据到缓冲区
        if (len <= 0) {                                  // 如果读取失败或数据读取完毕
            break;                                       // 跳出循环
        }
    } while (isET);                                      // 如果是边缘触发模式，继续读取
    return len;                                          // 返回读取的字节数
}

ssize_t HttpConn::write(int* saveErrno) {                // 向连接写入数据的函数
    ssize_t len = -1;                                    // 初始化写入长度为-1
    do {
        len = writev(fd_, iov_, iovCnt_);                // writev 允许一次性写入多个非连续内存块，而 iovec 结构数组正是用来描述这些内存块的位置和大小。
        if(len <= 0) {                                   // 如果写入失败
            *saveErrno = errno;                          // 保存错误码
            break;                                       // 跳出循环
        }
        
        //一次 writev 调用可能无法发送 iovec 中描述的所有数据。通过调整基地址和长度，程序可以在下一次调用时继续发送剩余的数据，而无需重新组织或复制这些数据。

        // 判断数据是否已经全部写入
        if(iov_[0].iov_len + iov_[1].iov_len == 0) {     // 如果数据已经全部写入
            break;                                       // 跳出循环
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len) { // 如果写入的数据超过第一部分
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);   //调整 iov_[1] 的基址，跳过已经写入的部分。
            iov_[1].iov_len -= (len - iov_[0].iov_len);                                 //调整 iov_[1] 的长度，减去已经写入的部分。
            if (iov_[0].iov_len) {                      //如果 iov_[0] 还有数据
                writeBuff_.RetrieveAll();               // 清空写缓冲区,因为其数据已经被完全写入。然后将 iov_[0].iov_len 设置为 0，表示没有更多数据要写入。
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*) iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);                   // 从写缓冲区移除已发送的数据
        }
    } while (isET || ToWriteBytes() > 10240);           // 在边缘触发模式下继续写入，或者待写数据大于10KB
    return len;
}

bool HttpConn::process() {                               // 处理读取的请求数据
    request_.Init();                                     // 初始化请求对象
    if (readBuff_.ReadableBytes() <= 0) {                // 如果没有可读数据
        return false;
    }
    else if (request_.parse(readBuff_)) {               // 解析请求
        LOG_DEBUG("%s", request_.path().c_str());       // 记录请求路径
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);    // 初始化响应对象
    }
    else {
        response_.Init(srcDir, request_.path(), false, 400);       // 如果解析失败，初始化错误响应        
    }

    response_.MakeResponse(writeBuff_);             // 构建响应并存入写缓冲区

    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek()); // 设置第一部分iovec的基址为写缓冲区的起始位置
    iov_[0].iov_len = writeBuff_.ReadableBytes();            // 设置第一部分iovec的长度为写缓冲区的可读字节数
    iovCnt_ = 1;                                             // 初始设置只有一个iovec结构

    if (response_.FileLen() > 0 && response_.File()) {       // 如果响应包含文件内容
        iov_[1].iov_base = response_.File();                 // 设置第二部分iovec的基址为文件内容的起始位置
        iov_[1].iov_len = response_.FileLen();               // 设置第二部分iovec的长度为文件的长度
        iovCnt_ = 2;                                         // 设置iovec结构数量为2
    }
    LOG_DEBUG("filesize:%d, %d to %d", response_.FileLen(), iovCnt_, ToWriteBytes());   // 记录调试信息

    return true;
}