#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; i++) {
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx); //加锁
                while (true) {
                    if (!pool->tasks.empty()) {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();    //处理任务期间，不需要持有锁
                        task();
                        locker.lock();      //处理完任务，重新尝试加锁
                    }
                    else if (pool->isClosed) break; // 如果线程池关闭，退出循环
                    else pool->cond.wait(locker);   
                }
            }).detach();        // 线程与 ThreadPool 对象分离
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if (static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
      
    }

    template<class F>       // 添加任务到线程池
    void AddTask(F && task) {
        {//限制locker的作用域，离开立即释放
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }
private:
    struct Pool {
        std::mutex mtx;                 // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;                  // 线程池是否关闭
        std::queue<std::function<void()>> tasks;    // 任务队列
    };
    std::shared_ptr<Pool> pool_;        // 指向 Pool 的共享指针
};
#endif