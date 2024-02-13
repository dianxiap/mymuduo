#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <assert.h>
#include <unistd.h>

using OnTimeCallback = std::function<void()>;
using ReleaseCallback = std::function<void()>;

// 定时任务类
class Timer
{
private:
    int _timeout;       // 当前定时任务的延迟时间
    bool _canceled;     // 当前定时任务是否被取消
    uint64_t _timer_id; // 定时器任务id
    OnTimeCallback _time_callback;
    ReleaseCallback _release_callback;

public:
    Timer(uint64_t time_id, int timeout)
        : _timer_id(time_id), _timeout(timeout), _canceled(false)
    {
    }
    ~Timer()
    {
        // 走到析构函数说明shared_ptr引用计数为0，因此要先把TimerQueue中保存的weak_ptr删除，在执行该任务
        if (_release_callback)
            _release_callback();

        // 定时任务执行函数设置了并且该任务没有被取消，则执行该任务
        if (_time_callback && !_canceled)
            _time_callback();
    }
    // 获取定时器任务的延迟时间
    int delay_time()
    {
        return _timeout;
    }
    void canceled()
    {
        _canceled = true;
    }
    // 设置执行任务回调函数
    void set_on_time_callback(const OnTimeCallback &cb)
    {
        _time_callback = cb;
    }
    // 设置取消weak_ptr回调函数
    void set_release_callback(const ReleaseCallback &cb)
    {
        _release_callback = cb;
    }
};

// 定时任务队列
class TimerQueue
{
private:
    using WeakTimer = std::weak_ptr<Timer>;
    using PtrTimer = std::shared_ptr<Timer>;
    using Bucket = std::vector<PtrTimer>;   // 时间轮每个节点都是一个桶，类似哈希桶
    using BucketList = std::vector<Bucket>; // 时间轮
    int _tick;                              // 秒指针，每秒向后移动一步，指到的Bucket任务都要被执行
    int _capacity;                          // 时间轮容量，最大定时时长
    BucketList _conns;
    std::unordered_map<uint64_t, WeakTimer> _times; // 保存所有定时任务对象的wead_ptr,这样在不影响对应shared_ptr的情况下，获取一个shared_ptr

public:
    TimerQueue()
        : _tick(0), _capacity(60), _conns(_capacity)
    {
    }
    // 检查一个定时任务是否存在
    bool has_timer(uint64_t id)
    {
        auto it = _times.find(id);
        if (it != _times.end())
            return true;
        return false;
    }
    // 添加一个定时任务
    void timer_add(const OnTimeCallback &cb, int delay, uint64_t id)
    {
        if (delay > _capacity || delay <= 0)
            return;
        PtrTimer timer(new Timer(id, delay));

        // 设置定时任务对象要执行的定时任务，会在对象被析构时执行
        timer->set_on_time_callback(cb);

        // 当前类成员_times保存了一份定时任务对象的weak_ptr，因此希望在析构时移除
        timer->set_release_callback(std::bind(&TimerQueue::remove_weaktimer_from_timerqueue, this, id));

        _times[id] = WeakTimer(timer);
        _conns[(_tick + delay) % _capacity].push_back(timer);
    }
    // 根据id刷新定时任务
    void timer_refresh(uint64_t id)
    {
        auto it = _times.find(id);
        assert(it != _times.end());

        // weak_ptr的lock函数可以找到一个同资源的shared_ptr
        int delay = it->second.lock()->delay_time();
        _conns[(_tick + delay) % _capacity].push_back(PtrTimer(it->second));
    }
    // 取消一个定时任务
    void timer_cancel(uint64_t id)
    {
        auto it = _times.find(id);
        assert(it != _times.end());
        PtrTimer pt = it->second.lock();
        if (pt)
            pt->canceled();
    }
    // tick指针每秒向后移动一步，指向的bucket将被释放，bucket中的任务将被执行
    void run_ontime_task()
    {
        _tick = (_tick + 1) % _capacity;
        _conns[_tick].clear();
    }

public:
    // 设置给Timer，最终定时任务执行完毕后，从timequeue移除timer信息的回调函数
    void remove_weaktimer_from_timerqueue(uint64_t id)
    {
        auto it = _times.find(id);
        if (it != _times.end())
            _times.erase(it);
    }
};

class TimerTest
{
private:
    int _data;

public:
    TimerTest(int data)
        : _data(data)
    {
        std::cout << "test 构造!\n";
    }
    ~TimerTest()
    {
        std::cout << "test 析构!\n";
    }
};

void del(TimerTest *t)
{
    delete t;
}

int main()
{
    TimerQueue tq;
    TimerTest *t = new TimerTest(10);

    int id = 3;
    tq.timer_add(std::bind(del, t), 5, id);

    for (int i = 0; i < 5; i++)
    {
        sleep(1);
        tq.timer_refresh(id);
        std::cout << "刷新3号任务" << std::endl;
        tq.run_ontime_task();
    }

    tq.timer_cancel(id);
    std::cout << "5s后3号任务不会被执行" << std::endl;
    
    while (1)
    {
        std::cout << "------------------" << std::endl;
        sleep(1);
        tq.run_ontime_task();
        if (tq.has_timer(id) == false)
        {
            std::cout << "任务3已被执行" << std::endl;
            break;
        }
    }
    // std::cout<<"5s后3号任务被执行"<<std::endl;
    // while(1)
    // {
    //     std::cout<<"------------------"<<std::endl;
    //     sleep(1);
    //     tq.run_ontime_task();
    //     if(tq.has_timer(id)==false)
    //     {
    //         std::cout<<"任务3已被执行"<<std::endl;
    //         break;
    //     }
    // }

    return 0;
}