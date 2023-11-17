#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    userCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {  // 单例模式，获取SqlConnPool的实例
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,      // 初始化连接池
                       const char* user, const char* pwd,
                       const char* dbName, int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {         // 循环创建数据库连接
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                user, pwd,
                                dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        connQue_.push(sql);  // 将连接加入到连接队列中
    }
    MAX_CONN_ = connSize;   // 设置最大连接数
    sem_init(&semId_, 0, MAX_CONN_);    // 初始化信号量
}

MYSQL* SqlConnPool::GetConn() {     // 获取一个数据库连接
    MYSQL *sql = nullptr;
    if (connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);              // 等待信号量, 信号量减一操作
    {
        lock_guard<mutex> locker(mtx_);// 加锁，保证线程安全
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL * sql) {   // 释放一个数据库连接
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);
}

void SqlConnPool::ClosePool() {             // 关闭连接池
    lock_guard<mutex> locker(mtx_);
    while (!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();    // 结束MySQL库
}

int SqlConnPool::GetFreeConnCount() {       // 获取空闲连接的数量
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}