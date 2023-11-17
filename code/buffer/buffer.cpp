#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;     // 返回缓冲区中可读的字节数，即写入位置减去读取位置
}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;  // 返回缓冲区中可写的字节数，即缓冲区总大小减去写入位置
}

size_t Buffer::PrependableBytes() const {
    return readPos_;                    // 返回缓冲区前端预留的字节数，即当前读取位置的索引
}


const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;      // 返回一个指针，指向当前的读取位置
}

//移动读取指针，标记已读取的数据。
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

//移动读取指针到指定位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

//清空缓冲区，重置读写位置。
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

//获取所有可读数据并转换为字符串，同时重置Buffer。
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

//返回指向当前写入位置的常量指针。
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

//返回指向当前写入位置的指针。
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

//更新写入位置，表示已经写入len长度的数据。
void Buffer::HasWritten(size_t len) {
    writePos_ +=  len;
}

//向缓冲区添加数据。
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

//从文件描述符fd读取数据到缓冲区。
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len) <= writable) {
        writePos_ += len;
    }
    else {
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

//将缓冲区数据写入文件描述符fd。
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if (len < 0) {
        *saveErrno = errno;
        return len;
    }
    readPos_ += len;
    return len;
}

char *Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

//返回指向缓冲区起始位置的指针。
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

//在缓冲区不足以容纳更多数据时扩展缓冲区。
void Buffer::MakeSpace_(size_t len) {
    if (WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    }
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}