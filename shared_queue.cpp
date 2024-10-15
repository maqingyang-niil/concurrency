#include <iostream>
#include "thread"
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
template<typename T>
class queue
{
private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    queue();
    queue(const queue&other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue=other.data_queue;
    }
    queue& operator=(const queue&)=delete;
    bool try_pop(T&value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value=data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T> ();
        std::shared_ptr<T> res(new T(data_queue.front()));
        data_queue.pop();
        return res;
    }
    void push(T new_val)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_val);
        data_cond.notify_one();
    }
    void wait_and_pop(T&value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value=data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res(new T(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool empty()const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};
