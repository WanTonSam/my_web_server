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
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator < (const TimerNode& t) {
        return expires < t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }

    void adjust(int id, int newExpires);
    
    void add(int id, int timeouto, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(size_t i);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;
};

#endif