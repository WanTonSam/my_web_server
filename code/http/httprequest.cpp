#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture", };
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1}, };

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;  // 设置初始解析状态为请求行
    header_.clear();        // 清空头部字段映射
    post_.clear();          // 清空 POST 字段映射
}

bool HttpRequest::IsKeepAlive() const {     //判断连接是否保持活跃
    if (header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";// 如果存在 "Connection" 字段且版本为 1.1，则保持连接
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";          // 搜索换行符，用于分割HTTP请求的各个部分
    if (buff.ReadableBytes() <= 0) {
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF +  2);
        std::string line(buff.Peek(), lineEnd);     // 从缓冲区获取一行数据
        switch (state_)
        {
            case REQUEST_LINE:
                if (!ParseRequestLine_(line)) {
                    return false;
                }
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if (buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                }
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if (lineEnd == buff.BeginWrite()) { break; }        // 判断是否已读取完所有数据
        buff.RetrieveUntil(lineEnd + 2);                    // 从缓冲区移除已解析的数据
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_() {
    if (path_ == "/") {      // 如果路径是根路径
        path_ = "/index.html";
    }
    else {
        for (auto &item : DEFAULT_HTML) {    // 遍历预设的HTML路径
            if (item == path_) {
                path_ += ".html";   // 添加html后缀
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {   // 解析请求行
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");     // 使用正则表达式解析请求行
    smatch subMatch;    // 存储正则匹配结果
    if (regex_match(line, subMatch, patten)) {  // 如果匹配成功
        method_ = subMatch[1];  // 设置请求方法
        path_ = subMatch[2];    // 设置请求路径
        version_ = subMatch[3]; // 设置HTTP版本
        state_ = HEADERS;       // 更改解析状态为头部解析
        return true;
    }
    LOG_ERROR("RequestLine error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2]; // 存储头部键值对
    }
    else {
        state_ = BODY;  // 更改解析状态为正文解析
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;    // 设置正文内容
    ParsePost_();   // 解析 POST 请求
    state_ = FINISH;    // 更改解析状态为解析完成
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch)  {  // 将字符转换为十六进制的方法
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;   // 处理大写字母
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;   // 处理小写字母
    return ch;
}
    
void HttpRequest::ParsePost_() {            // 解析 POST 请求
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {// 如果是 POST 请求且内容类型为 application/x-www-form-urlencoded
        ParseFromUrlencode_();  // 解析 URL 编码的 POST 数据
        if (DEFAULT_HTML_TAG.count(path_)) {    // 如果路径在 HTML 标签映射中
            int tag = DEFAULT_HTML_TAG.find(path_)->second; // 获取 HTML 标签
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);   // 判断是否为登录操作
                if (UserVerify(post_["username"], post_["password"], isLogin)) {    // 验证用户
                    path_ = "/welcome.html";
                }
                else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencode_() {    // 解析 URL 编码
    if (body_.size() == 0) { return; }

    string key, value;  // 存储键值对
    int num = 0;
    int n = body_.size();    // 正文长度
    int i = 0, j = 0;       // 迭代器
                                    /*  POST http://www.example.com HTTP/1.1    
                                        Content-Type:application/x-www-form-urlencoded;charset=utf-8
                                        title=test&sub%5B%5D=1&sub%5B%5D=2&sub%5B%5D=3 */

    for (; i < n; i++) {
        char ch = body_[i]; // 获取字符
        switch (ch) {   // 根据字符进行不同处理
            case '=':
                key = body_.substr(j, i - j);       
                j = i + 1;
                break;
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body_.substr(j, i - j);
                j = i + 1;
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default :  
                break;
        }
    }
    assert( j <= i);
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string& name, const string& pwd, bool isLogin) {
    if (name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin) { flag = true; }
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);

        if (isLogin) {
            if (pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    if (!isLogin && flag == true) {
        LOG_DEBUG("register!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}