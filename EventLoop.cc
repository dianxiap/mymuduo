#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个eventloop,指向当前线程，每个线程独有一份
__thread EventLoop* t_loopInThisThread=nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs=10000;

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtd<0)
    {
        LOG_FATAL("eventfd errno:%d\n",errno);
    }
    return evtd;
}

EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupfd_(createEventfd())
    ,wakeupChannel_(new Channel(this,wakeupfd_))
{
    LOG_DEBUG("Eventloop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another Eventloop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
    }
    else 
    {
        t_loopInThisThread=this;
    }

    //设置wakeupfd的事件类型以及事件发生后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handelRead,this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();

}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableALL();
    wakeupChannel_->remove();
    ::close(wakeupfd_);
    t_loopInThisThread=nullptr;

}

void EventLoop::handelRead()
{
    uint64_t one =1;
    ssize_t n=read(wakeupfd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",n);
    }
}