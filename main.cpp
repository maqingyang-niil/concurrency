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
class join_threads
{
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& thread_):threads(thread_){}
    ~join_threads()
    {
        for (auto& t:threads)
        {
            if (t.joinable())
                t.join();
        }
    }
};
template<typename T>
class thread_safe_queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node*next;
        node():next(NULL){}
    };
    std::mutex head_mutex;
    node*head;
    std::mutex tail_mutex;
    node*tail;
    std::condition_variable data_cond;
    node*get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    node*pop_head()
    {
        node*const old_head=head;
        head=old_head->next;
        return old_head;
    }
    node*try_pop_head()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head==get_tail())
        {
            return NULL;
        }
        return pop_head();
    }
    node*try_pop_head(T&value)
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head==get_tail())
        {
            return NULL;
        }
        value=*head->data;
        return pop_head();
    }
    std::unique_lock<std::mutex> wait_for_data()
    {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock,[&]{return head!=get_tail();});
        return head_lock;
    }
    node*wait_pop_head()
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }
    node*wait_pop_head(T&value)
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        value=*head->data;
        return pop_head();
    }
public:
    thread_safe_queue():head(new node),tail(head){}
    thread_safe_queue(const thread_safe_queue&other)=delete;
    thread_safe_queue*operator=(const thread_safe_queue&other)=delete;
    ~thread_safe_queue()
    {
        while(head)
        {
            node*const old_head=head;
            head=old_head->next;
            delete old_head;
        }
    }
    std::shared_ptr<T> try_pop()
    {
        node*const old_head=try_pop_head();
        if (!old_head)
        {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }
    bool try_pop(T&value)
    {
        node*const old_head=try_pop_head(value);
        if (!old_head)
        {
            return false;
        }
        delete old_head;
        return true;
    }
    std::shared_ptr<T> wait_and_pop()
    {
        node*const old_head=wait_pop_head();
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }
    void wait_and_pop(T&value)
    {
        node*const old_head=wait_pop_head(value);
        delete old_head;
    }
    void push(T new_value)
    {
        std::shared_ptr<T> new_data(new T(new_value));
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail->data=new_data;
            tail->next=p.get;
            tail=p.release();
        }
        data_cond.notify_one();
    }
    void empty()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head==get_tail());
    }
    std::map<Key,Value> get_map()const
    {
        std::vector<std::unique_lock<boost::shared_mutex>> locks;
        for(unsigned i=0;i<buckets.size();++i)
        {
            locks.push_back(std::unique_lock<boost::shared_mutex>(buckets[i].mutex));
        }
        std::map<Key,Value> res;
        for (unsigned i=0;i<buckets.size();++i)
        {
            for (bucket_iterator it=buckets[i].data.begin();it!=buckets[i].data.end();++it) {
                res.insert(*it);
            }
        }
        return res;
    }
};
class thread_pool
{
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;
    void work_thread()
    {
        while(!done)
        {
            std::function<void()> task;
            if (work_queue.try_pop(task))
            {
                task();
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
public:
    thread_pool(): joiner(threads),done(false)
    {
        unsigned const thread_count=std::thread::hardware_concurrency();
        try
        {
            for (unsigned i=0;i<thread_count;++i)
            {
                threads.push_back(std::thread(&thread_pool::work_thread,this));
            }
        }catch()
        {
            done=true;
            throw;
        }
    }
    ~thread_pool()
    {
        done=true;
    }

    template<typename FunctionType>
    void submit(FunctionType f)
    {
        work_queue.push(std::function<void()>(f));
    }
};










