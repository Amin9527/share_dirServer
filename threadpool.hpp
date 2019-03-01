#include<iostream>
#include<queue>
//#include"utils.hpp"
#include<unistd.h>
#include<pthread.h>

typedef bool (*Handler)(int socket);

class HttpTask
{
    //http请求处理的任务
    //包含一个成员就是socket
    //包含一个任务处理接口
    private:
        int _cli_sock;
        Handler TaskHandler;
    public:
        HttpTask():_cli_sock(-1){}

        HttpTask(int cli_sock , Handler taskhandler):_cli_sock(cli_sock),TaskHandler(taskhandler){}

        void SetHttpTask(int sock , Handler taskhandler)
        {
            _cli_sock = sock;
            TaskHandler = taskhandler;
        }

        void handler()
        {
            TaskHandler(_cli_sock);  //任务处理函数
        }
};

class ThreadPool
{
    //线程池类
    //创建指定数量的线程
    //创建一个线程安全的任务队列
    //提供任务的入队、出队、线程池的初始化、销毁接口
    private:
        int _thr_sum; //当前线程池的线程数量
        int _max_thr; //当前线程池的最大线程数
        std::queue<HttpTask> _task_queue;
        pthread_mutex_t _lock; //锁
        pthread_cond_t _cond;  //条件变量

    private:
        static void* thr_start(void* arg)  //自定义处理函数
        {
            pthread_detach(pthread_self()); //线程分离，pthread_self()返回当前线程自己的pid
            ThreadPool* tp = (ThreadPool*)arg;
            while(1)
            {

                //std::cout<<"new thread!"<<std::endl;
                
                tp->QueueLock();  //线程访问任务队列时 上锁
                while(tp->QueueIsEmpty()) //如果任务队列为空
                {
                    tp->ThreadWait(); //阻塞等待
                }

                HttpTask ht = tp->QueueFront(); //如果不为空，取出队列头部任务
                tp->QueuePop();  //将任务移出队列
                tp->QueueUnLock(); //解锁
                ht.handler(); //执行处理函数
            }

            return nullptr;
        }
    public:

        ThreadPool(int max_thr):_max_thr(max_thr){}

        void ThreadWait()
        {
            pthread_cond_wait(&_cond , &_lock);
        }

        void QueueLock()
        {
            pthread_mutex_lock(&_lock);
        }

        void QueueUnLock()
        {
            pthread_mutex_unlock(&_lock);
        }

        void QueuePop()
        {
            _task_queue.pop();
        }

        HttpTask QueueFront()
        {
            return _task_queue.front();
        }

        bool ThreadPoolInit()  //初始化
        {
            pthread_mutex_init(&_lock,nullptr); //初始化锁
            pthread_cond_init(&_cond,nullptr); //初始化条件变量
            for(int i=0;i<_max_thr;++i)   //创建五个_max_thr个线程
            {
                pthread_t tid;
                int ret =pthread_create(&tid,nullptr,thr_start,(void*)this); //自定义处理函数，传入的参数为this指针
                  if(ret != 0) //成功返回0
                  {
                      std::cout<<"thread create error"<<std::endl;
                      return false;
                  }
            }
            return true;
        }

        bool PushTask(HttpTask &tt) //向任务队列加入任务
        {
            QueueLock();  //上锁
            _task_queue.push(tt);
            QueueUnLock(); 
            pthread_cond_signal(&_cond); //放入任务后发送条件信号变量
            return true;
        }

        /*
        //线程安全的任务出队，因为任务的出队是在线程接口中调用，但是
        //线程接口中在出队之前就会进行加锁，因此这里不需要加锁
        bool PopTask(HttpTask &tt)
        {
            tt = _task_queue.front();
            _task_queue.pop();
        }
        */

        bool QueueIsEmpty()
        {
            return _task_queue.size() == 0 ? true : false;
        }

        void ThreadPoolStop()
        {
            pthread_mutex_destroy(&_lock); //销毁
            pthread_cond_destroy(&_cond);
        }
};
