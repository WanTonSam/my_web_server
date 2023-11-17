#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <arpa/inet.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
//C++标准库中的一个时钟类型，提供最高可能的时间测量精度。
typedef std::chrono::high_resolution_clock Clock;
//是一个持续时间类型，表示毫秒级的时间跨度。
typedef std::chrono::milliseconds MS;
//是一个表示时间点的类型，它是从某个固定的时间点（如系统启动或UNIX纪元）到当前的时间跨度。
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;             // 定时器的唯一标识符
    TimeStamp expires;  // 定时器的到期时间
    TimeoutCallBack cb; // 到期时的回调函数
    bool operator < (const TimerNode& t) {
        return expires < t.expires; // 定义小于运算符，以到期时间为比较基准
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }

    void adjust(int id, int newExpires);    // 调整定时器的到期时间
    
    void add(int id, int timeouto, const TimeoutCallBack& cb);// 添加定时器

    void doWork(int id);            // 执行指定id的定时器的回调函数

    void clear();                   // 清除所有定时器

    void tick();                    // 处理所有到期的定时器任务

    void pop();                     // 移除堆顶定时器

    int GetNextTick();              // 获取距离下一次定时任务的时间

private:
    void del_(size_t i);            // 删除指定位置的定时器

    void siftup_(size_t i);         // 向上调整堆

    bool siftdown_(size_t index, size_t n); // 向下调整堆

    void SwapNode_(size_t i, size_t j);     // 交换堆中的两个节点

    std::vector<TimerNode> heap_;           // 存储定时器的最小堆

    std::unordered_map<int, size_t> ref_;   // id到堆索引的映射
};

#endif