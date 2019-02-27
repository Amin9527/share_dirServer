#include"threadpool.hpp"


bool test(int i)
{
    std::cout<<i+i<<":  "<<pthread_self()<<std::endl;
    return true;
}

int main()
{
    ThreadPool* p = new ThreadPool(5);
    p->ThreadPoolInit();
    HttpTask ht;
    int i=0;
    while(1)
    {
        ht.SetHttpTask(i,test);
        p->PushTask(ht);
        sleep(1);
        ++i;
    }
    return 0;
}
