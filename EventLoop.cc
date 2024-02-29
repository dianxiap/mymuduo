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

// 开启事件循环
void EventLoop::loop()
{
    looping_=true;
    quit_=false;

    LOG_INFO("EventLoop %p start looping \n",this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd 一种是client的fd，一种是wakeupfd
        pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel* channel: activeChannels_)
        {
            //Poller监听那些channle发生了事件，然后上报给eventloop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);        
        }
        //执行当前Eventloop事件循环需要处理的回调操作
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_=false;
}

// 退出事件循环
// 1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_=true;

    // 如果是在其他线程中，调用quit
    if(!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread()) //在当前的loop线程中，执行cb
    {
        cb();
    }
    else //在非当前loop线程中执行cb，就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程。执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop线程
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调 
    if(!isInLoopThread()||callingPendingFunctors_)
    {
        wakeup();   //唤醒loop所在线程
    }
}

 // 用来唤醒loop所在的线程
 //向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one=1;
    ssize_t n=write(wakeupfd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("Eventloop::wakeup() writes %lu bytes instead of 8\n",n);
    }
}

// EventLoop的方法 -》poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

 // 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_=true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor: functors)
    {
        functor();  //执行当前loop所要执行的回调操作
    }

    callingPendingFunctors_=false;
}

