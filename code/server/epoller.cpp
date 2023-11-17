#include "epoller.h"

Epoller::Epoller(int maxEvent): epollFd_(epoll_create(512)), events_(maxEvent){ // Epoller类的构造函数
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {      // 添加文件描述符到epoll监控
    if (fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;    // 设置文件描述符
    ev.events = events; // 设置事件类型
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);    // 添加到epoll监控，成功返回true，失败返回false
}

bool Epoller::ModFd(int fd, uint32_t events) {      // 修改epoll中的文件描述符事件
    if (fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {                       // 从epoll中删除文件描述符
    if (fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs) {                  // 等待epoll事件，返回就绪的事件数
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {           // 获取指定索引的事件文件描述符
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {       // 获取指定索引的事件类型
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}