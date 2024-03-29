#ifndef __M_UTILS_H__
#define __M_UTILS_H__

#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <unistd.h>
#include <signal.h>  // signal()
#include <fcntl.h>  // open(),O_RDONLY
#include <dirent.h>  //
#include <stdio.h>  // printf()、fprintf()
#include <stdlib.h>  // sleep()
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sstream>  // stringstream

#define WWWROOT "www"
#define MAX_PATH 256
#define MAX_HTTPHDR 4096
#define MAX_BUF 4096

#define LOG(...) do{fprintf(stdout, __VA_ARGS__); fflush(stdout);}while(0)


std::unordered_map<std::string, std::string> g_err_desc = {
    {"200" , "OK"},
    {"400" , "Bad Resquest"},
    {"403" , "Forbidden"},
    {"404" , "Not Found"},
    {"405" , "Method Not Allowed"},
    {"413" , "Request Entity Too Large"},
    {"500" , "Internal Server Error"}
};

std::unordered_map<std::string, std::string> g_mime_type = {
    {"txt",    "application/octet-stream"},
    {"html",   "text/html"},
    {"htm",    "text/html"},
    {"jpg",    "application/octet-stream"},
    {"zip",    "application/zip"},
    {"mp3",    "audio/mpeg"},
    {"mpeg",   "video/mpeg"},
    {"unknow", "application/octet-stream"}
};

class Utils
{
    public:
        // 分割字符串
        static int Split(std::string &src, const std::string &seg, std::vector<std::string> &list)
        {
            size_t start = 0;
            size_t pos = 0;
            int sum = 0;
            while(start < src.length())
            {
                pos = src.find(seg, start);  // 在src中从start下标处找seg，找到返回该位置下标，没找到返回npos(-1)
                if(pos == std::string::npos)  // 没找到退出循环
                    break;
                list.push_back(src.substr(start, pos - start));  // 在src中从start下标开始复制pos-start个字符，尾插到list
                start = pos + seg.length();
                ++sum;
            }
            if(start < src.length())
            {
                list.push_back(src.substr(start));  // 在src中从start下标开始复制到末尾，尾插到list中
                ++sum;
            }
            return sum;
        }

        static const std::string GetErrDesc(const std::string &code)
        {
            auto it = g_err_desc.find(code);
            if(it == g_err_desc.end())
            {
                return "Unknow Error";
            }
            return it->second;
        }

        // 将时间转换为一定格式的字符串
        static void TimeToGMT(time_t t, std::string &gmt)
        {
            struct tm *mt = gmtime(&t);  // 将当前时间转换为GMT时间
            char tmp[128]= {0};
            int len = strftime(tmp, 127, "%a ,%d %b %Y %H:%M:%S GMT" , mt);
            gmt.assign(tmp, len);
        }

        // 数字转字符串
        static void DigitToStr(int64_t num, std::string &str)
        {
            std::stringstream ss; 
            ss << num;  // 将数字num放入ss
            str = ss.str();
        }

        // 字符串转数字
        static int64_t StrToDigit(const std::string &str)
        {
            int64_t num;
            std::stringstream ss;
            ss << str;  // 将字符串str放入ss
            ss >> num;  // 输出为数字
            return num;
        }

        static void MakeETag(uint64_t size, int64_t ino, int64_t mtime, std::string &etag)
        {
            std::stringstream ss;
            // "ino-size-mtime"
            ss<<"\"";
            ss<<std::hex<<ino;
            ss<<"-";
            ss<<std::hex<<size;
            ss<<"-";
            ss<<std::hex<<mtime;
            ss<<"\"";
            etag = ss.str();
        }

        static void GetMime(const std::string &file, std::string &mime)
        {
            size_t pos;
            pos = file.find_last_of(".");  // 找最后一个点
            if(pos == std::string::npos)
            {
                mime = g_mime_type["unknow"];
                return;
            }
            std::string suffix = file.substr(pos + 1);
            auto it = g_mime_type.find(suffix);
            if (it == g_mime_type.end())
            {
                mime = g_mime_type["unknow"];
                return;
            }
            else
            {
                mime = it -> second;
            }
        }
};

class RequestInfo
{
    // 包含HttpRequest解析出的请求信息
    public:
        std::string _method;  // 请求方法
        std::string _version;  // 协议版本
        std::string _path_info;  // 资源路径
        std::string _path_phys;  // 资源实际路径
        std::string _query_string;  // 查询字符串
        std::unordered_map<std::string, std::string> _hdr_list;  // 整个头部信息中的键值对
        struct stat _st;  // 获取文件信息

    public:
        std::string _err_code;  // 错误号码

    public:
        void SetError(const std::string &code)
        {
            _err_code = code;
        }

    public:
        bool RequestIsCGI()  // 判断请求类型
        {
            LOG("_query_string : %s\r\n\n", _query_string.c_str());
            if((_method == "GET" && !_query_string.empty()) || (_method == "POST"))  // 如果请求类型为GET但是请求字符串不为空为CGI请求
                return true;
            return false;
        }
};

class HttpRequest
{
    // http数据的接收接口
    // http数据的解析接口
    // 对外提供能够获取处理结果的接口
    private:
        int _cli_sock;
        std::string _http_header;
        RequestInfo info;

    public:
        HttpRequest(int sock): _cli_sock(sock){}
        bool RecvHttpHeader() // 接收http请求头（请求行+请求头部），并且保存到_http_header
        {
            char buf[MAX_HTTPHDR];
            while(1)
            {
                std::cout << "---recv function start---" << std::endl;
                int ret = recv(_cli_sock, buf, MAX_HTTPHDR, MSG_PEEK);  // 参数MSG_PEEK，只读不拿，不删除数据，数据还在。ret为实际取到的数据字节数
                // LOG("recv function return result = %s\n", ret);
                std::cout << "recv function return = " << ret << std::endl;
                if(ret <= 0)  // =0,对端关闭连接
                {
                    if(errno == EINTR || errno == EAGAIN)  // EINTR被信号打断
                    {
                        continue;
                    }
                    info.SetError("500");
                    return false;   
                }

                // 在buf接收到的数据中找空行，找到返回首次出现"\r\n\r\n"位置的指针，没找到返回nullptr
                // 即：请求行 + 请求头部 + 空行 + 请求正文，找这里的空行
                char *ptr = strstr(buf, "\r\n\r\n");

                LOG("buf context = %s\n", buf);

                // 没找到，但是接收缓冲区buf已经满了，说明：请求行+请求头部太大，返回error code ：413
                if((ptr == nullptr) && (ret == MAX_HTTPHDR))
                {
                    info.SetError("413");
                    return false;
                }

                // 没找到，但是接收缓冲区buf没满，说明：还在接收，则继续等待接收
                else if((ptr == nullptr) && (ret < MAX_HTTPHDR))
                {
                    usleep(1000);
                    continue;
                }

                // 找到之后，获取请求行+请求头部的大小
                int hdr_len = ptr - buf;

                _http_header.assign(buf, hdr_len);  // 截取字符串，在buf里截取hdr_len长度的字符串
                recv(_cli_sock, buf, hdr_len + 4, 0);  // 再读一次，请求将头拿走
                LOG("header:%s\r\n\n", _http_header.c_str());
                break;
            }
            return true;
        }

        bool ParseHttpHeader()  // 解析http请求头
        {
            std::vector<std::string> v_info;
            Utils::Split(_http_header, "\r\n", v_info);  // 将_http_header按照"\r\n"进行分割

            std::vector<std::string> v_first_line;  
            Utils::Split(v_info[0], " ", v_first_line);  // 将首行按照" "分割
            info._method = v_first_line[0];  // 请求方法
            info._version = v_first_line[2];  // 协议及版本


            std::vector<std::string> v_path;  // 请求行的url
            Utils::Split(v_first_line[1], "?", v_path);  // 将请求URL按照"?"分割
            if(v_path.size() == 2)
            {
                info._query_string = v_path[1];  // 查询字符串
            }
            info._path_info = v_path[0];  // 资源路径
            info._path_phys = "./www";
            info._path_phys += info._path_info;  // 实际资源路径

            // 将头部信息存进键值对unordered_map
            std::vector<std::string> v_pair;
            for(size_t i = 1; i < v_info.size(); ++i)
            {
                Utils::Split(v_info[i], ": ", v_pair);
                info._hdr_list[v_pair[0]] = v_pair[1];
                v_pair.resize(0);
            }

            return true;
        }

        RequestInfo GetRequestInfo()  // 向外提供解析结果
        {
            return info;
        }
};

class HttpResponse
{
    private:
        int _cli_sock;
        // ETag: "inode-fsize-mtime"\r\n
        std::string _etag;  // 查看文件是否被修改过，请求的文件是否是源文件
        std::string _mtime; // 文件最后一次修改时间
        std::string _date;  // 当前系统的时间
        std::string _fsize; // 文件大小
        std::string _mime;  // 文件类型

    public:
        HttpResponse(int sock): _cli_sock(sock){}

        // 初始化一些请求响应信息
        bool InitResponse(RequestInfo& req_info)
        { 
            std::string dir = req_info._path_phys;

            // 获取参数一指定的路径下的文件或者文件夹的信息，保存到参数二中
            stat(dir.c_str(), &req_info._st);
            // Last_Modified: 
            Utils::TimeToGMT(req_info._st.st_mtime, _mtime);
            // ETag: 
            Utils::MakeETag(req_info._st.st_size, req_info._st.st_ino, req_info._st.st_mtime, _etag);
            // Date:  
            time_t t = time(NULL);
            Utils::TimeToGMT(t, _date);
            // 文件大小
            Utils::DigitToStr(req_info._st.st_size, _fsize);
            // 文件类型
            Utils::GetMime(req_info._path_phys, _mime);
            return true;
        }

        // 文件是否为一个目录
        bool FileIsDir(RequestInfo& info)
        {
            if(info._st.st_mode & S_IFDIR)
            {
                if(info._path_info.back() != '/')
                {
                    info._path_info.push_back('/');
                }
                if(info._path_phys.back() != '/')
                {
                    info._path_phys.push_back('/');
                }
                return true;
            }
            return false;
        }

        // 错误处理响应
        bool ErrHandler(RequestInfo &info)
        {
            std::string rsp_header;  // 响应头信息
            // 首行:协议版本 状态码 状态描述\r\n
            // 头部:Content-Length Date
            // 空行:
            // 正文:rsp_body = "<html><body><h1>404<h1></body></html>"
            rsp_header = info._version + " " + info._err_code + " ";
            rsp_header += Utils::GetErrDesc(info._err_code) + "\r\n";

            time_t t = time(NULL);  // 获取系统当前时间
            std::string gmt;
            Utils::TimeToGMT(t , gmt);
            rsp_header += "Data: " + gmt + "\r\n";

            std::string rsp_body;
            rsp_body = "<html><body><h1>" + info._err_code;
            rsp_body += "<h1></body></html>";
            std::string cont_len;
            Utils::DigitToStr(rsp_body.length(), cont_len);

            rsp_header += "Content-Length: " + cont_len + "\r\n\r\n";

            send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
            send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
            return true;
        }

        bool SendData(const std::string &buf)
        {
            if(send(_cli_sock, buf.c_str(), buf.length(), 0) < 0)
            {
                return false;
            }
            return true;
        }

        bool SendCData(const std::string &buf)
        {
            if(buf.empty())
            {
                SendData("0\r\n\r\n");
            }
            std::stringstream ss;
            ss << std::hex << buf.length() << "\r\n";

            SendData(ss.str());
            SendData(buf);
            SendData("\r\n");

            return true;
        }

        // 文件列表功能
        bool ProcessList(RequestInfo &info)
        {
            // 组织头部:
            // 首行
            // Content-Type：text/html\r\n
            // ETag: \r\n
            // Data: \r\n
            // Transfer-Encoding: chunked\r\n  （分块传输）1.1才有
            // Connection：close\r\n
            // 正文: 
            // 每一个目录下的文件都要组织一个html标签信息
            std::string rsp_header;
            rsp_header = info._version + "200 OK\r\n";
            rsp_header += "Content-Type: text/html\r\n";
            rsp_header += "Connection: close\r\n";
            if(info._version == "HTTP/1.1")
            {
                rsp_header += "Transfer-Encoding: chunked\r\n";  // 只在1.1版本下有
            }
            rsp_header += "ETag: " + _etag + "\r\n";
            rsp_header += "Last-Modified: " + _mtime + "\r\n";
            rsp_header += "Date: " + _date + "\r\n\r\n";
            SendData(rsp_header);

            std::string rsp_body;
            rsp_body = "<html><head>";
            rsp_body += "<title>Amin" + info._path_info + "</title>";
            rsp_body += "<meta charset = 'UTF-8'>";
            rsp_body += "</head><body>";
            rsp_body += "<h1>File Transmission Module" + info._path_info + "</h1>";

            // 上传文件form表单
            // 这个表单创建了一个输入型的选择文件按钮和一个上传按钮
            // 规定了action为upload_file，method为POST，那当我们选择好文件点击上传按钮后，
            // 浏览器访问服务器的http请求的请求行里方法即为POST，访问资源即为upload_file
            rsp_body += "<form action='/upload_file' method='POST' enctype='multipart/form-data'>";
            rsp_body += "<input type='file' name='FileUpLoad' />";  // 选择文件按钮
            rsp_body += "<input type='submit' value='Upload' />";  // 上传按钮
            rsp_body += "</form>";

            rsp_body += "<hr /><ol>";  // <hr />横线
            SendCData(rsp_body);

            std::string path = info._path_phys;
            struct dirent **p_dirent = NULL;
            // 获取目录下的每一个文件，组织html信息，chunke传输
            int num = scandir(info._path_phys.c_str(), &p_dirent, 0, alphasort);

            for(int i = 0; i < num; i++)
            {
                std::string file_html;
                std::string file = info._path_phys + p_dirent[i]->d_name;
                struct stat st;
                if(stat(file.c_str(), &st) < 0)
                {
                    continue;
                }
                std::string mtime;
                Utils::TimeToGMT(st.st_mtime, mtime);
                std::string mime;
                Utils::GetMime(p_dirent[i]->d_name, mime);
                std::string fsize;
                Utils::DigitToStr(st.st_size / 1024, fsize);
                file_html += "<li><strong><a href='" + info._path_info;  // a href组织链接、strong加粗
                file_html += p_dirent[i]->d_name;
                file_html += "'>";
                file_html += p_dirent[i]->d_name;
                file_html += "</a></strong>";
                file_html += "<br />";
                file_html += "<small>";
                file_html += "modified: " + mtime + "<br />";  // "<br />" 换行
                file_html += mime + " - " + fsize + " kbytes"; //
                file_html += "<br /><br /></small></li>";  // li有序(在ol里)
                SendCData(file_html);
            }
            rsp_body = "</ol><hr /></body></html>";
            SendCData(rsp_body);
            SendCData("");

            return true;
        }


        // 文件下载功能
        bool ProcessFile(RequestInfo &info)
        {
            std::string rsp_header;
            rsp_header = info._version + "200 OK\r\n";
            rsp_header += "Content-Type: " + _mime + "\r\n";
            rsp_header += "Connection: close\r\n";
            rsp_header += "Content-Length: " + _fsize + "\r\n";
            rsp_header += "ETag: " + _etag + "\r\n";
            rsp_header += "Last-Modified: " + _mtime + "\r\n";
            rsp_header += "Date: " + _date + "\r\n\r\n";
            SendData(rsp_header);

            int fd = open(info._path_phys.c_str(), O_RDONLY);
            if(fd < 0)
            {
                info._err_code = "400";
                ErrHandler(info);
                return false;
            }

            std::cout << "download file send head context = " << std::endl;

            int rlen = 0;
            char tmp[MAX_BUF];  // 4096
            while((rlen = read(fd, tmp, MAX_BUF)) > 0)
            {
                send(_cli_sock, tmp, rlen, 0);
                std::cout << "download file send context = " << std::endl;

                // tmp[rlen] = '\0';
                // SendData(tmp);
            }
            close(fd);
            return true;
        }

        // CGI请求处理
        bool ProcessCGI(RequestInfo &info)
        {
            // 使用外部程序完成CGI请求处理 --文件上传
            // 将http头信息和正文信息全部交给子进程
            // 使用环境变量传递头信息
            // 使用管道传递正文数据
            // 使用管道接收cgi程序的处理结果
            // 流程：创建管道，创建子进程，设置子进程环境变量，程序替换
            
            int in[2];  // 用于向子进程传递正文信息
            int out[2];  // 用于从子进程中读取处理结果
            if(pipe(in) || pipe(out))  // 创建匿名管道
            {
                info._err_code = "500";
                ErrHandler(info);
                return false;
            }

            // 创建子进程
            // fork之后，创建出子进程，父子进程将fork之后的代码都执行一遍
            // fork给父进程返回子进程的pid，给子进程返回0（子进程的pid不为0）。即：fork后的代码中pid为0的是子进程
            int pid = fork();
            if(pid  < 0)
            {
                info._err_code ="500";
                ErrHandler(info);
                return false;
            }
            else if(pid == 0)  // 子进程
            {
                // int setenv() 设置环境变量 #include<stdlib.h>
                setenv("METHOD", info._method.c_str(), 1);  // 方法
                setenv("VERSION", info._version.c_str(), 1);  // 版本
                setenv("PATH_INFO", info._path_info.c_str(), 1);  // 路径
                setenv("QUERY_STRING", info._query_string.c_str(), 1); 
                for(auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
                {
                    setenv(it->first.c_str(), it->second.c_str(), 1);
                }
                close(in[1]);  // 关闭子进程的写
                close(out[0]);  // 关闭子进程的读
                dup2(in[0], 0);  // 子进程将从标准输入读取正文数据
                dup2(out[1], 1);  // 子进程直接打印处理结果传递给父进程

                // execl函数：跳转执行另一个指定的可执行程序
                // 参数一：要执行的程序路径
                // 参数二：程序名称，没什么用
                // 第三个参数开始为执行程序需要的参数，程序 *argv[]接收参数
                // 最后一个参数为null，告知execl参数列表的结束
                // execl(info._path_phys.c_str(), info._path_phys.c_str(), NULL);
                std::string execl_file = "./" + info._path_info;
                execl(execl_file.c_str(), info._path_info.c_str(), NULL);
                exit(0);
            }
            close(in[0]);  // 关闭父进程的读
            close(out[1]);  // 关闭父进程的写

            // 走下来的就是父进程
            // 1.通过in管道将正文数据传递给子进程
            auto it = info._hdr_list.find("Content-Length");

            // 没有找到Content-Length则不需要提交正文数据给子进程
            if(it != info._hdr_list.end())
            {
                char buf[MAX_BUF]={0};
                int64_t content_len = Utils::StrToDigit(it->second);  // 获取到正文的长度

                int tlen = 0;
                while(tlen < content_len)
                {
                    int len = MAX_BUF > (content_len - tlen) ? (content_len - tlen) : MAX_BUF;  // 保证不多接收
                    int rlen = recv(_cli_sock, buf, len,0);
                    if(rlen < 0)
                    {
                        // 响应错误信息给客户端
                        return false;
                    }
                    if(write(in[1], buf, rlen) < 0)
                    {
                        return false;
                    }
                    tlen += rlen;
                }
            }
            // 2.通过out管道读取子进程的处理结果直到返回0
            // 3.将处理结果组织http数据，响应给客户端
            // 组织头部信息
            std::string rsp_header;
            rsp_header = info._version + "200 OK\r\n";
            rsp_header += "Content-Type: text/html\r\n";
            rsp_header += "Connection: close\r\n";
            rsp_header += "ETag: " + _etag + "\r\n";
            rsp_header += "Last-Modified: " + _mtime + "\r\n";
            rsp_header += "Date: " + _date + "\r\n\r\n";
            SendData(rsp_header);

            while(1)
            {
                char buf[MAX_BUF] = {0};
                int rlen = read(out[0], buf, MAX_BUF);
                if(rlen == 0)
                {
                    break;
                }
                send(_cli_sock, buf, rlen, 0);
            }

            std::string rsp_body;
            rsp_body = "<html><body><h1>UPLOAD SUCCESS!</h1></body></html>";
            SendData(rsp_body);
            close(in[1]);
            close(out[0]);
            return true;
        }
};
#endif
