#include <iostream>
#include <vector>
#include "thread"
#include <list>
#include <algorithm>
#include <functional>
#include <numeric>
#include <mutex>
class hierarchical_mutex
{
    std::mutex internal_mutex;
    unsigned long const hierarchical_level;
    unsigned long previous_hierarchical_level;
    static thread_local unsigned long this_thread_hierarchical_level;
    void check_for_hierarchy_violation()
    {
        if (this_thread_hierarchical_level<=hierarchical_level)
            throw std::logic_error("hierarchy_violation");
    }
    void update_hierarchy_level()
    {
        previous_hierarchical_level=this_thread_hierarchical_level;
        this_thread_hierarchical_level=hierarchical_level;
    }
public:
    explicit hierarchical_mutex(unsigned long val):hierarchical_level(val),previous_hierarchical_level(0){}
    void lock()
    {
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_level();
    }
    void unlock()
    {
        this_thread_hierarchical_level=previous_hierarchical_level;
        internal_mutex.unlock();
    }
    bool try_lock()
    {
        check_for_hierarchy_violation();
        if (!internal_mutex.try_lock())
            return false;
        update_hierarchy_level();
        return true;
    }
};