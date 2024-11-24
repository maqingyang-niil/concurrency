#include <iostream>
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <utility>
#include <vector>
#include <map>
#include <share.h>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <future>
#include <numeric>
class thread_interrupted: public std::exception
{
    const char* what() const noexcept override
    {
        return "Thread interrupted";
    }
};
void interruption_point();
class interrupt_flag
{
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::condition_variable_any* thread_cond_any;
    std::mutex set_clear_mutex;
public:
    interrupt_flag():thread_cond(0),thread_cond_any(0){}
    void set() {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if (thread_cond) {
            thread_cond->notify_all();
        } else if (thread_cond_any) {
            thread_cond_any->notify_all();
        }
    }
    template<typename Lockable>
    void wait(std::condition_variable_any& cv,Lockable& lk)
    {
        struct custom_lock
        {
            interrupt_flag* self;
            Lockable& lk;
            custom_lock(interrupt_flag* self_,std::condition_variable_any& cond,Lockable& lk_):self(self_),lk(lk_)
            {
                self->set_clear_mutex.lock();
                self->thread_cond_any=&cond;
            }
            void unlock()
            {
                lk.unlock();
                self->set_clear_mutex.unlock();
            }
            void lock()
            {
                self->set_clear_mutex.lock();
                lk.lock();
            }
            ~custom_lock()
            {
                self->thread_cond_any=0;
                self->set_clear_mutex.unlock();
            }
        };
        custom_lock cl(this,cv,lk);
        interruption_point();
        cv.wait(cl);
        interruption_point();
    }
    bool is_set()const;
};
thread_local interrupt_flag this_thread_interrupt_flag;
void interruption_point()
{
    if (this_thread_interrupt_flag.is_set())
        throw thread_interrupted();
}
template<typename Lockable>
void interruptible_wait(std::condition_variable& cv,Lockable& lk)
{
    this_thread_interrupt_flag.wait(cv,lk);
}
class interruptible_thread
{
    std::thread internal_thread;
    interrupt_flag* flag;
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f)
    {
        struct wrapper
        {
            FunctionType m_f;
            wrapper(FunctionType& f):m_f(f){}
            void operator()(interrupt_flag** flag_ptr,std::mutex* m,std::condition_variable* c)
            {
                {
                    std::lock_guard<std::mutex> lk(*m);
                    *flag_ptr=&this_thread_interrupt_flag;
                }
                c->notify_one();
                try
                {
                    m_f();
                }catch(thread_interrupted&){}
            }
        };
        std::mutex flag_mutex;
        std::condition_variable flag_condition;
        flag=0;
        internal_thread=std::thread(wrapper(f),&flag,&flag_mutex,&flag_condition);
        std::unique_lock ul(flag_mutex);
        while(!flag)
        {
            flag_condition.wait(ul);
        }
    }
    void interrupt()
    {
        if (flag)
        {
            flag->set();
        }
    }
    void join();
    void detach();
    bool joinable()const;
};









