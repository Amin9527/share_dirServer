#include "threadpool.hpp"
#include "utils.hpp"

#define MAX_THREAD 5
#define MAX_LISTEN 7

class HttpServer
{
    // 建立一个TCP服务端程序，接收新连接
    // 为新连接组织一个线程池任务，添加到线程池当中
    private:
        int _serv_sock;
        ThreadPool *_tp;

    private:
        static bool HttpHandler(int sock)  // http任务的处理函数
        {
            RequestInfo info;        // 实例化包含头部信息的对象
            HttpRequest req(sock);   // 实例化解析头部信息的对象
            HttpResponse rsp(sock);  // 实例化处理请求的对象
           
            if(req.RecvHttpHeader() == false)  // 接收http请求头
            {
                goto out;
            }

            if(req.ParseHttpHeader() == false)  // 解析http请求头
            {
                goto out;
            }

            info = req.GetRequestInfo();
             
            if(info.RequestIsCGI())  // 判断是否为CGI请求
            {
                // std::cout << "request is CGI!" << std::endl;
                rsp.ProcessCGI(info);  //CGI请求处理
            }
            else
            {
                // rsp.FileHandler(info);  // 文件请求处理（目录列表/文件下载）
                rsp.InitResponse(info);
                if(rsp.FileIsDir(info))  // 判断是否为目录文件
                {
                    rsp.ProcessList(info);  // 目录列表展示
                }
                else
                {
                    rsp.ProcessFile(info);  // 文件下载
                }
            }
            
            close(sock);
            return true;
        out:
            rsp.ErrHandler(info);  // 处理错误响应
            close(sock);
            return false;
        }
        
    public:
        HttpServer() : _serv_sock(-1), _tp(NULL){}
        
        // 完成tcp服务器socket的初始化，线程池初始化
        bool HttpServerInit(std::string &ip, int port)
        {
            _serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if(_serv_sock < 0)
            {
                LOG("sock error");
                return false;
            }

            int opt = 1;
            setsockopt(_serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));  // 服务器断开立即可以启动，不用等待
            
            sockaddr_in lst_addr;
            lst_addr.sin_family = AF_INET;
            lst_addr.sin_port = htons(port);
            lst_addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len = sizeof(sockaddr_in);
            if(bind(_serv_sock, (sockaddr*)&lst_addr, len) < 0)
            {
                LOG("bind error :%s\n", strerror(errno));
                close(_serv_sock);
                return false;
            }

            if(listen(_serv_sock, MAX_LISTEN) < 0)
            {
                LOG("listen error: %s\n", strerror(errno));
                close(_serv_sock);
                return false;
            }

            _tp = new ThreadPool(MAX_THREAD);  // 创建线程池
            if(_tp == nullptr)
            {
                LOG("thread pool malloc error\n");
                return false;
            }
            _tp->ThreadPoolInit();  // 初始化线程池
            return true;
        }

        // 开始获取客户端新连接--创建任务，任务入队
        bool Start()
        {
            while(1)
            {
                sockaddr_in cli_addr;
                socklen_t len = sizeof(sockaddr_in);
                int cli_sock = accept(_serv_sock,(sockaddr*)&cli_addr,&len);
                if(cli_sock < 0)
                {
                    LOG("accept error: %s\n",strerror(errno));
                    continue;
                }

                HttpTask ht;
                ht.SetHttpTask(cli_sock , HttpHandler);
                _tp->PushTask(ht);
            }
            return true;
        }
};

int main(int argc, char* argv[])
{
    argc = 3;
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    HttpServer hs;

    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号，发送消息时，对端如果关闭连接，会产生该信号，默认为终止进程

    hs.HttpServerInit(ip, port);
    hs.Start();
}
