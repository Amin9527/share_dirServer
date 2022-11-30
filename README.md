# share_dirServer

## 操作命令

#### 生成可执行程序
- 执行 ``make``命令
之后生成两个可执行程序 ``HttpServer`` 和 ``upload_file``，
``upload_file`` 为主进程fork出的子进程通过execl函数执行的程序。进行文件的上传

#### 启动
- ``./HttpServer 0.0.0.0 <port>``
- ``nohup ./HttpServer 0.0.0.0 <port> &``

#### 访问
浏览器下：``<部署服务的机器ip>:<port>``

## 实现原理

### class HttpServer
全局只有一个HttpServer类对象，它在最外层，作用是在执行HttpServerInit函数时，创建并维护一个socket套接字和一个ThreadPool线程池类对象

#### HttpServerInit()
- socket：create、bind、listen，还没有accept
- 维护一个ThreadPool类对象，创建并维护一个5个（可修改）线程的线程池，全部被pthread_mutex_t锁和pthread_cond_t条件变量阻塞在std::queue\<HttpTask\>队列，等待队列中元素的到来

一切就绪之后启动：Start
#### Start()
- 循环阻塞在client_socket = socket.accept()函数，等待client连接。每当等到一个client时，就创建一个HttpTask任务类对象，并用client_socket和任务处理函数HttpHandler进行初始化，ht = HttpTask(client_socket, HttpHandler)。之后通过维护的ThreadPool类对象，将该ht对象push到ThreadPool对象中维护的任务队列std::queue\<HttpTask\>，然后通知阻塞在任务队列的线程们获取任务，进行后续操作

线程取到任务后执行：HttpHandler(client_socket)
#### static HttpServer::HttpHandler(int client_socket)
```c++
// http协议格式

// 请求方法 URL 协议版本  // 请求行
// 头部字段名：值         // 请求头部
// ......
// 头部字段名：值
//                       // 空行
// ......                // 请求体
```

每个处理函数中维护了三个对象：RequestInfo、HttpRequest、HttpResponse
##### RequestInfo
RequestInfo是用来保存维护http请求的头部信息

##### HttpRequest
每次进到处理函后，首先通过该对象函数进行http请求的解析，将解析结果保存到RequestInfo类对象中

##### HttpResponse
然后根据RequestInfo中保存的解析信息判断，是get还是post请求，是否是CGI请求，然后执行相应的处理

Response回复完client后，该线程继续阻塞在std::queue<HttpTask>队列，等待下一次任务的到来

测试重复commit1
测试重复commit2
测试重复commit3