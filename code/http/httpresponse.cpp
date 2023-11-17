#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {  // 定义静态成员：HTTP 状态码到描述的映射
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {    // 定义静态成员：HTTP 状态码到错误页面路径的映射
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}

HttpResponse::~HttpResponse() {
    UnmapFile();    // 析构函数中取消文件映射
}

void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if (mmFile_) { UnmapFile(); }   // 如果已有映射文件，先取消映射
    code_ = code;                    // 设置状态码
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) { // 构建 HTTP 响应
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;                // 如果文件不存在或路径是目录，设置状态码为 404
    }
    else if (!(mmFileStat_.st_mode & S_IROTH)) {    // 如果文件没有读权限，设置状态码为 403
        code_ = 403;
    }
    else if (code_ == -1) {
        code_ = 200;
    }
    ErrorHtml_();           // 生成错误页面
    AddStateLine_(buff);    // 添加状态行
    AddHeader_(buff);       // 添加头部
    AddConten_(buff);       // 添加内容
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return  mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {       // 根据状态码生成错误页面路径
    if (CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {     // 添加 HTTP 响应状态行
    string status;
    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;     // 默认设置状态码为 400
        status = CODE_STATUS.find(code_)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff) {        // 添加 HTTP 响应头部
    buff.Append("Connection: ");
    if (isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddConten_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        ErrorConten(buff, "File NotFound!");
        return;
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1) {
        ErrorConten(buff, "File NotFound!");    // 文件未找到错误处理
        return;
    }
    mmFile_ = (char*)mmRet;     // 设置内存映射文件
    close(srcFd);               // 关闭文件描述符
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {    // 取消文件映射
    if (mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

string HttpResponse::GetFileType_() {   // 获取文件类型
    string ::size_type idx = path_.find_last_of('.');
    if (idx == string::npos) {
        return "text/plain";    // 如果没有找到文件后缀，默认为纯文本
    }
    string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorConten(Buffer& buff, string message) {  // 生成错误内容
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        status = "Bad Request"; // 默认状态描述
    }
    body += to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size())  + "\r\n\r\n");
    buff.Append(body);
}