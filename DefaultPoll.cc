#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if(::getenv("MUDUM_USE_POLL"))
    {
        return nullptr; //生成poll的实例
    }
    else 
    {
        return new EPollPoller(loop);  //new EPollPoller(loop); //生成epoll
    }
}