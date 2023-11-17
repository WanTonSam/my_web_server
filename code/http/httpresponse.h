#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);// 初始化 HttpResponse 对象
    void MakeResponse(Buffer& buff);    // 构建 HTTP 响应内容
    void UnmapFile();        // 取消文件映射
    char* File();           // 获取文件数据
    size_t FileLen() const; // 获取文件长度
    void ErrorConten(Buffer& buff, std::string message);    // 生成错误响应内容
    int Code() const { return code_; }   // 获取 HTTP 状态码
private:
    void AddStateLine_(Buffer& buff);   // 添加状态行
    void AddHeader_(Buffer& buff);      // 添加头部
    void AddConten_(Buffer& buff);      // 添加内容
    void ErrorHtml_();                  // 生成错误
    std::string GetFileType_();         // 获取文件类型

    int code_;                          // HTTP状态码
    bool isKeepAlive_;                  // 是否保持连接

    std::string path_;                  // 请求路径
    std::string srcDir_;                // 源文件目录

    char* mmFile_;                      // 内存映射的文件数据
    struct stat mmFileStat_;            // 文件状态信息

    static const std::unordered_map<std::string ,std::string> SUFFIX_TYPE;   // 静态映射：文件后缀类型
    static const std::unordered_map<int, std::string> CODE_STATUS;           // 静态映射：HTTP状态码对应的状态
    static const std::unordered_map<int, std::string> CODE_PATH;             // 静态映射：HTTP状态码对应的路径
};

#endif //HTTP_RESPONSE_H