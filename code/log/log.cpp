#include "log.h"

using namespace std;

Log::Log() {
    linecount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if (writeThread_ && writeThread_->joinable()) {
        while (!deque_->empty()) {
            deque_->flush();    // 等待队列清空
        }
        deque_->Close();        // 关闭队列
        writeThread_->join();   // 等待写线程结束
    }
    if (fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();    // 刷新缓冲区
        fclose(fp_);
    }
}

int Log::GetLevel() {   // 获取日志等级
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) { // 设置日志等级
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

void Log::init(int level = 1, const char* path, const char* suffix,  // 初始化日志系统
    int maxQueueSize) {
        isOpen_ = true;
        level_ = level;
        if (maxQueueSize > 0) { // 如果设置了最大队列大小，使用异步写入
            isAsync_ = true;
            if (!deque_) {
                unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
                deque_ = move(newDeque);

                std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
                writeThread_ = move(NewThread);
            }
        }
        else {
            isAsync_ = false;    // 否则使用同步写入
        }

        linecount_ = 0;

        time_t timer = time(nullptr);
        struct tm *sysTime = localtime(&timer);
        struct tm t = *sysTime;
        path_ = path;
        suffix_ = suffix;
        char fileName[LOG_NAME_LEN] = {0};
        snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
                path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
        toDay_ = t.tm_mday;

        {
            lock_guard<mutex> locker(mtx_);
            buff_.RetrieveAll();
            if (fp_) {
                flush();
                fclose(fp_);
            }

            fp_ = fopen(fileName, "a");
            if (fp_ == nullptr) {
                mkdir(path_, 0777);
                fp_ = fopen(fileName, "a");
            }
            assert(fp_ != nullptr);
        }
}

void Log::write(int level, const char* format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);  // 获取当前时间
    struct tm t = *sysTime;
    va_list vaList;

    if (toDay_ != t.tm_mday || (linecount_ && (linecount_ % MAX_LINES == 0))) {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            linecount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (linecount_ / MAX_LINES), suffix_);
        }

        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        linecount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if (isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        }
        else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.Append("[debug]: ", 9);
            break;
        case 1:
            buff_.Append("[info] : ", 9);
            break;
        case 2:
            buff_.Append("[warn] : ", 9);
            break;
        case 3:
            buff_.Append("[error]: ", 9);
            break;
        default:
            buff_.Append("[info] : ", 9);
            break;
    }
}

void Log::flush() {
    if (isAsync_) {
        deque_->flush();     // 如果是异步，刷新队列
    }
    fflush(fp_);    // 刷新文件缓冲区
}

void Log::AsynWrite_() {     // 异步写入处理
    string str = "";
    while (deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);    // 将字符串写入文件
    }
}

Log* Log::Instance() {   // 获取 Log 类的单例
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {    // 日志刷新线程函数
    Log::Instance()->AsynWrite_();
}