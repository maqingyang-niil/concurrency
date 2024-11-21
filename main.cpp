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
    typedef typename std::function<void()> data_type;
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










