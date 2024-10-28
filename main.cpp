#include <iostream>
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>
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
    queue():head(new node),tail(head){}
    queue(const queue&other)=delete;
    queue*operator=(const queue&other)=delete;
    ~queue()
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
};