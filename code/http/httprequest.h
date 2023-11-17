#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {  // 枚举类型：解析 HTTP 请求的不同状态
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    enum HTTP_CODE {    // 枚举类型：HTTP 请求的可能结果
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() { Init();}
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;// 获取 POST 请求中的数据
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;   // 检查是否保持连接

private:
                                    //用于解析 HTTP 请求的不同部分
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);
                                    //用于进一步解析请求路径和请求正文
    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencode_();
                                    // 静态函数，用于用户验证
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);
                                    //用于存储 HTTP 请求的状态和组成部分
    PARSE_STATE state_;

    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};

#endif