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
class work_stealing_queue
{
private:
    typedef std::function<void()> data_type;
    std::deque<data_type> the_queue;
    mutable std::mutex the_mutex;
public:
    work_stealing_queue();
    work_stealing_queue(const work_stealing_queue&other)=delete;
    work_stealing_queue& operator=(const work_stealing_queue&other)=delete;
    void push(data_type const&data)
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        the_queue.push_front(data);
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }
    bool try_pop(data_type& res)
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        if (the_queue.empty())
        {
            return false;
        }
        res=std::move(the_queue.front());
        the_queue.pop_front();
        return true;
    }
    bool try_steal(data_type& res)
    {
        std::lock_guard<std::mutex> lock(the_mutex);
        if (the_queue.empty())
            return false;
        res=std::move(the_queue.back());
        the_queue.pop_back();
        return true;
    }
};
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
    typedef std::function<void()> task_type;
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue>> queues;
    std::vector<std::thread> threads;
    join_threads joiner;
    static thread_local work_stealing_queue* local_work_queue;
    static thread_local unsigned my_index;
    void worker_thread(unsigned my_index_)
    {
        my_index=my_index_;
        local_work_queue=queues[my_index].get();
        while(!done)
        {
            run_pending_task();
        }
    }
    bool pop_task_from_local_queue(task_type& task)
    {
        return local_work_queue&&local_work_queue->try_pop(task);
    }
    bool pop_task_from_pool_queue(task_type& task)
    {
        return pool_work_queue.try_pop(task);
    }
    bool pop_task_from_other_thread_queue(task_type& task)
    {
        for (unsigned i=0;i<queues.size();++i)
        {
            unsigned const index=(my_index+1+i)%queues.size();
            if (queues[index]->try_pop(task))
            {
                return true;
            }
        }
        return false;
    }
public:
    thread_pool(): joiner(threads),done(false)
    {
        unsigned const thread_count=std::thread::hardware_concurrency();
        try
        {
            for (unsigned i=0;i<thread_count;++i)
            {
                queues.push_back(std::unique_ptr<work_stealing_queue>(std::make_unique<work_stealing_queue>()));
                threads.push_back(std::thread(&thread_pool::worker_thread,this,i));
            }
        }catch()
        {
            done=true;
            throw;
        }
    }
    template<typename ResultType>
    using task_handle=std::future<ResultType>;

    template<typename FunctionType>
    task_handle<typename std::result_of<FunctionType()>::type> submit(FunctionType f)
    {
        typedef typename std::result_of<FunctionType()>::type result_type;

        std::packaged_task<result_type()> task(f);
        task_handle<result_type> res(task.get_future());
        if (local_work_queue)
        {
            local_work_queue->push(std::move(task));
        }
        else
        {
            pool_work_queue.push(std::move(task));
        }
        return res;
    }
    void run_pending_task()
    {
        task_type task;
        if (pop_task_from_local_queue(task)|| pop_task_from_pool_queue(task)|| pop_task_from_other_thread_queue())
        {
            task();
        }
        else
        {
            std::this_thread::yield();
        }
    }
    ~thread_pool()
    {
        done=true;
    }
};









