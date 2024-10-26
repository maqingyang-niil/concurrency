#include <iostream>
#include "thread"
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <future>
#include <chrono>
#include <atomic>

template<typename T>
class queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node*next;
        node():next(NULL){}
    };
    node*head;
    std::mutex head_mutex;
    node*tail;
    std::mutex tail_mutex;
    node*get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    node*pop_head()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head==get_tail())
        {
            return NULL;
        }
        node const*old_node=head;
        head=head->next;
        return old_node;
    }
public:
    queue():head(new node),tail(head){};
    queue(const queue&other)=delete;
    queue*operator=(const queue&other)=delete;
    ~queue()
    {
        while(head)
        {
            node const*old_node=head;
            head=head->next;
            delete old_node;
        }
    }
    std::shared_ptr<T> try_pop()
    {
        node*old_node=head;
        if (!old_node)
            return std::shared_ptr<T>();
        std::shared_ptr<T> const res(old_node->data);
        delete old_node;
        return res;
    }
    void push(T value)
    {
        std::shared_ptr<T> new_data(new T(value));
        std::unique_ptr<node> p(new node);
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data=new_data;
        tail->next=p.get();
        tail=p.release();
    }
};
