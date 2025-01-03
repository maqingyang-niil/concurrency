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
unsigned const max_hazard_pointers=100;
struct hazard_pointer
{
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};
hazard_pointer hazard_pointers[max_hazard_pointers];
class hp_owner
{
    hazard_pointer*hp;
public:
    hp_owner(hp_owner const&)=delete;
    hp_owner operator=(hp_owner const&)=delete;
    hp_owner():hp(NULL)
    {
        for (unsigned i=0;i<max_hazard_pointers;++i)
        {
            std::thread::id old_id;
            if (hazard_pointers[i].id.compare_exchange_strong(old_id,std::this_thread::get_id()))
            {
                hp=&hazard_pointers[i];
                break;
            }
        }
        if(!hp)
        {
            throw std::runtime_error("no hazard pointers available");
        }
    }
    std::atomic<void*>& get_pointer()
    {
        return hp->pointer;
    }
    ~hp_owner()
    {
        hp->pointer.store(NULL);
        hp->id.store(std::thread::id());
    }

};
std::atomic<void*>& get_hazard_pointer_for_current_thread()
{
    thread_local static hp_owner hazard;
    return hazard.get_pointer();
}
bool outstanding_hazard_pointers_for(void*p)
{
    for(unsigned i=0;i<max_hazard_pointers;++i)
    {
        if (hazard_pointers[i].pointer.load()==p)
        {
            return true;
        }
        return false;
    }
}
template<typename T>
void do_delete(void*p)
{
    delete static_cast<T*>(p);
}
struct data_to_reclaim
{
    void* data;
    std::function<void(void*)> deleter;
    data_to_reclaim* next;
    template<typename T>
    data_to_reclaim(T*p):data(p),deleter(&do_delete<T>),next(0){}
    ~data_to_reclaim()
    {
        deleter(data);
    }
};
std::atomic<data_to_reclaim*> nodes_to_reclaim;
void add_to_reclaim_list(data_to_reclaim* node)
{
    node->next=nodes_to_reclaim.load();
    while(!nodes_to_reclaim.compare_exchange_weak(node->next,node));

}
template<typename T>
void reclaim_later(T*data)
{
    add_to_reclaim_list(new data_to_reclaim(data));
}
void delete_nodes_with_no_hazard()
{
    data_to_reclaim* current=nodes_to_reclaim.exchange(NULL);
    while(current)
    {
        data_to_reclaim* const next=current->next;
        if (!outstanding_hazard_pointers_for(current->data))
        {
            delete current;
        }
        else
        {
            add_to_reclaim_list(current);
        }
        current=next;
    }
}
template<typename T>
class stack
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        node*next;
        node(T const& data_):data(std::make_shared<T>(data_)){}
    };
    std::atomic<unsigned> thread_in_pop;
    std::atomic<node*> to_be_delete;
    std::atomic<node*> head;

    static void delete_nodes(node*nodes)
    {
        while(nodes)
        {
            node*next=nodes->next;
            delete nodes;
            nodes=next;
        }
    }
public:
    void push(T const&value)
    {
        node* const new_node=new node(value);
        new_node->next=head.load();
        while(!head.compare_exchange_weak(new_node->next,new_node));
    }
    std::shared_ptr<T> pop()
    {
        ++thread_in_pop;
        node* old_head=head.load();
        while(old_head&&!head.compare_exchange_weak(old_head,old_head->next));
        std::shared_ptr<T> res;
        if (old_head)
        {
            res.swap(old_head->data);
        }
        node* node_to_delete=to_be_delete.load();
        if(!--thread_in_pop)
        {
            if(to_be_delete.compare_exchange_strong(node_to_delete,NULL))
            {
                delete_nodes(node_to_delete);
            }
            delete old_head;
        }
        else if(old_head)
        {
            old_head->next=node_to_delete;
            while(!to_be_delete.compare_exchange_weak(old_head->next,old_head));
        }
        return res;
    }
    std::shared_ptr<T> pop_1()
    {
        std::atomic<void*>& hp=get_hazard_pointer_for_current_thread();
        node* old_head=head.load();
        do {
            node* temp;
            do {
                temp=old_head;
                hp.store(old_head);
                old_head=head.load();
            }while(old_head!=temp);
        } while (old_head&&!head.compare_exchange_strong(old_head,old_head->next));
        hp.store(NULL);
        std::shared_ptr<T> res;
        if (old_head)
        {
            res.swap(old_head->data);
            if (outstanding_hazard_pointers_for(old_head))
            {
                reclaim_later(old_head);
            }
            else
            {
                delete old_head;
            }
            delete_nodes_with_no_hazard();
        }
        return res;
    }

};






















