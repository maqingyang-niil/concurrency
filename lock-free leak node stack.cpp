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
template<typename T>
class stack
{
    struct node
    {
        std::shared_ptr<T> data;
        node*next;
        node(T const& data_):data(std::make_shared<T>(data_)){}
    };
    std::atomic<node*> head;
public:
    void push(T const&value)
    {
        node* const new_node=new node(value);
        new_node->next=head.load();
        while(!head.compare_exchange_weak(new_node->next,new_node));
    }
    std::shared_ptr<T> pop()
    {
        node* old_node=head.load();
        while(old_node&&!head.compare_exchange_weak(old_node,old_node->next));
        return old_node?old_node->data:std::shared_ptr<T>();
    }
};







































