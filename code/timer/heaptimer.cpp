#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());  // 断言i在合法范围内
    size_t j = (i - 1) / 2;  // 计算父节点索引
    while(j >= 0) {  // 当i不是根节点时
        if(heap_[j] < heap_[i]) { break; }  // 如果父节点小于当前节点，停止上浮
        SwapNode_(i, j);  // 交换当前节点和父节点
        i = j;  // 更新当前节点为父节点，继续上浮检查
        j = (i - 1) / 2;  // 重新计算父节点索引
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);  // 交换堆中的两个节点
    ref_[heap_[i].id] = i;   // 更新节点i的索引映射
    ref_[heap_[j].id] = j;  // 更新节点j的索引映射
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;   // 开始的节点索引
    size_t j = i * 2 + 1;   // 左子节点索引
    while (j < n) {         // 当左子节点在范围内时
        if (j + 1 < n && heap_[j + 1] < heap_[j]) j++;// 选取左右子节点中较小的那个
        if (heap_[i] < heap_[j]) break; // 如果当前节点小于子节点，停止下沉
        SwapNode_(i, j);    // 交换当前节点和子节点
        i = j;  // 更新当前节点索引为子节点
        j = i * 2 + 1;  // 更新左子节点索引
    }
    return i > index;   // 返回是否发生了下沉
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    }
    else {
         /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if (!siftdown_(i, heap_.size())) {  //如果新的到期时间更晚
            siftup_(i); // 如果新的到期时间更早
        }
    }
}

void HeapTimer::doWork(int id) {
    //删除指定id节点，并触发回调函数
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    //删除指定位置的节点
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    //将要删除的节点换到队尾，然后调整堆
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode_(i, n);
        if (!siftdown_(i, n)) {
            siftup_(i);
        }
    }

    //队尾元素删除
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    //调整指定id的节点
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    //清除超时节点
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

// 获取距离下一次定时任务的时间
int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) { res = 0; }
    }
    return res;
}