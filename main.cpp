#include <iostream>
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
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
#include "thread_safe_stack.h"
template<typename T>
struct sorter
{
    struct chunk_to_sort
    {
        std::list<T> data;
        bool end_of_data;
        std::promise<std::list<T>> promise;
        chunk_to_sort(bool done=false):end_of_data(done){}
    };
    stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    unsigned const max_thread_count;
    sorter():max_thread_count(std::thread::hardware_concurrency()-1){}
    ~sorter()
    {
        for (unsigned i=0;i<threads.size();++i)
        {
            chunks.push(std::move(chunk_to_sort(true)));
        }
        for (unsigned i=0;i<threads.size();++i)
        {
            threads[i].join();
        }
    }
    bool try_sort_chunk()
    {
        boost::shared_ptr<chunk_to_sort> chunk=chunks.pop();
        if (chunk)
        {
            if (chunk->end_of_data)
            {
                return false;
            }
            sort_chunk(chunk);
        }
        return true;
    }
    std::list<T> do_sort(std::list<T>& chunk_data)
    {
        if (chunk_data.empty())
        {
            return chunk_data;
        }
        std::list<T> result;
        result.splice(result.begin(),chunk_data,chunk_data.begin());
        T const& partition_val=*result.begin();
        typename std::list<T>::iterator divide_point=std::partition(chunk_data.begin(),chunk_data.end(),less_than<T>(partition_val));
        chunk_to_sort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.end(),chunk_data,chunk_data.begin(),divide_point);
        std::future<std::list<T>> new_lower=new_lower_chunk.promise.get_future();
        chunks.push(std::move(new_lower_chunk));
        if (threads.size()<max_thread_count)
        {
            threads.push_back(std::thread(&sorter<T>::sort_thread,this));
        }
        std::list<T> new_higher(do_sort(chunk_data));
        result.splice(result.end(),new_higher);
        while(!new_lower.is_ready())
        {
            try_sort_chunk();
        }
        result.splice(result.begin(),new_lower.get());
        return result;
    }
    void sort_chunk(boost::shared_ptr<chunk_to_sort> const& chunk)
    {
        chunk->promise.set_value(do_sort(chunk->data));
    }
    void sort_thread()
    {
        while(try_sort_chunk())
        {
            std::this_thread::yield();
        }
    }
};
template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    sorter<T> s;
    return s.do_sort(input);
}





















