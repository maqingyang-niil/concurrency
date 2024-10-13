#include <iostream>
#include <vector>
#include "thread"
#include <algorithm>
#include <functional>
#include <numeric>
template<typename Iterator,typename T>
struct accumulate_block
{
    void operator()(Iterator first,Iterator last,T&result)
    {
        result=std::accumulate(first,last,result);
    }
};

template<typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init)
{
    unsigned long const length=std::distance(first,last);
    if (!length)
        return init;
    unsigned long const min_per_thread=25;
    unsigned long const max_threads=(length+min_per_thread-1)/min_per_thread;
    unsigned long const hardware_threads=std::thread::hardware_concurrency();
    unsigned long const num_threads=std::min(hardware_threads!=0?hardware_threads:2,max_threads);
    unsigned long const block_size=length/num_threads;
    std::vector<T> result(num_threads);
    std::vector<std::thread> threads(num_threads-1);
    Iterator block_start=first;
    for (unsigned long i=0;i<(num_threads-1);++i)
    {
        Iterator block_end=block_start;
        std::advance(block_end,block_size);
        threads[i]=std::thread(accumulate_block<Iterator,T>(),block_start,block_end,std::ref(result[i]));
        block_start=block_end;
    }
    accumulate_block<Iterator,T>()(block_start,last,result[num_threads-1]);
    std::for_each(threads.begin(),threads.end(),std::mem_fn(&std::thread::join));
    return std::accumulate(result.begin(),result.end(),init);
}
int main() {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 22, 33, 44, 55, 66, 77, 88, 34, 324, 545, 66, 7, 68, 4253,
                            464, 3, 332, 34, 43, 34, 67, 75};
    unsigned long i = parallel_accumulate(vec.begin(), vec.end(), 0);
    std::cout<<i;
}
