//
// Created by Umasou on 2021/6/23.
//

#include "time_finder.h"
#include "cache.h"
#include "log.h"

/**
 *参数：需要替换的ip集合
 *操作：表示这些ip不再重要，因此不再用于训练，把他们从ip_last_addr中删除
 **/
void Time_finder::repl_ip(vector<uint64_t> erase_ips)
{
    //更新逻辑过于暴力,可以修改
    //erase
    for (auto it : erase_ips)
    {
        this->ip_last_addr.erase(it);
    }
}

/**
 * 参数：本次访问的address 对应的cache_line 的第一个地址
 * 返回值：该ip历史情况访问地址对中 key为addr时，下一个最有可能访问的地址，如果不存在，则返回-1
 * 问题：是否需要判断一下 置信度
 **/
uint64_t Time_finder::find_next_addr(uint64_t addr)
{
    for (auto addr_it : trained_time_recorder)
    {
        if (addr == addr_it.start_addr)
        {
            return addr_it.next_addr.addr;
        }
    }
    return -1;
}

/**
 * 参数：ip 和 本次访问address对应的 cache_line 的第一个地址
 * 返回值：预测需要预取的address（或cache_line）集合
 * 问题：是否返回值中会有重复的
 **/
vector<uint64_t> Time_finder::predict(uint64_t cache_line)
{
    vector<uint64_t> pf_addrs;
    uint64_t start_addr = cache_line;
    for (int i = 0; i < PREFETCH_DEGREE; ++i)
    {
        uint64_t next_addr = this->find_next_addr(start_addr);
        if (-1 == next_addr || find(pf_addrs.begin(), pf_addrs.end(), next_addr) != pf_addrs.end())
        {
            break;
        }
        else
        {
            pf_addrs.push_back(next_addr);
            start_addr = next_addr;
        }
    }
    return pf_addrs;
}

/**
 * 参数：本次访问的ip 和 address
 * 操作：更新ip最后访问的address
 **/
void Time_finder::update_ip_last_addr(uint64_t ip, uint64_t addr)
{
    //增加ip选择的功能，可以参考wuhao(micro 19)
    this->ip_last_addr[ip] = addr;
}

void Time_finder::update_trained_time_recorder(uint64_t start_addr, uint64_t next_addr) {
    int index = -1;
    for (int i = 0; i < this->trained_time_recorder.size(); ++i)
    {
        if (this->trained_time_recorder[i].start_addr == start_addr)
        {
            index = i;
            break;
        }
    }
    if (index != -1)//以start_addr开始的ip已经训练完成了
    {
        if (this->trained_time_recorder[index].next_addr.addr == next_addr)
        {
            if (this->trained_time_recorder[index].next_addr.conf < DEFAULT_CONF)
            {
                this->trained_time_recorder[index].next_addr.conf += 1;
            }
            for (auto it : this->trained_time_recorder)
            {
                it.lru += 1;
            }
            this->trained_time_recorder[index].lru = 0;
        }
        else
        {
            if (this->trained_time_recorder[index].next_addr.conf == 0)
            {
                this->trained_time_recorder.erase(trained_time_recorder.begin() + index);//从训练完成里面剔除
                update_training_time_recorder(start_addr, next_addr);//加入训练的time_finder中
            }
            else
            {
                this->trained_time_recorder[index].next_addr.conf -= 1;
            }
        }
    }
    else
    { //从来没有记录过该ip对应start_addr的下一个访问地址
        for (auto it : this->trained_time_recorder)
        {
            it.lru += 1;
        }
        //is not full
        if (this->trained_time_recorder.size() < TRAINED_ENTRY_NUM)
        {
            Next_addr nextAddr;
            nextAddr.addr = next_addr;
            nextAddr.conf = 1;
            Addr_pair addrPair;
            addrPair.start_addr = start_addr;
            addrPair.next_addr = nextAddr;
            addrPair.lru = 0;
            this->trained_time_recorder.push_back(addrPair);
        }
        else
        {
            //find victim
            int victim_index = 0;
            for (int i = 0; i < this->trained_time_recorder.size(); ++i)
            {
                if (this->trained_time_recorder[i].lru > this->trained_time_recorder[victim_index].lru || (this->trained_time_recorder[i].lru == this->trained_time_recorder[victim_index].lru && this->trained_time_recorder[i].next_addr.conf < this->trained_time_recorder[victim_index].next_addr.conf))
                {
                    victim_index = i;
                }
            }
            //add new pair
            Next_addr nextAddr;
            nextAddr.addr = next_addr;
            nextAddr.conf = 1;
            this->trained_time_recorder[victim_index].lru = 0;
            this->trained_time_recorder[victim_index].start_addr = start_addr;
            this->trained_time_recorder[victim_index].next_addr = nextAddr;
        }
    }
}

void Time_finder::update_training_time_recorder(uint64_t start_addr, uint64_t next_addr) {
    int index = -1;
    for (int i = 0; i < this->training_time_recorder.size(); ++i)
    {
        if (this->training_time_recorder[i].start_addr == start_addr)
        {
            index = i;
            break;
        }
    }
    if (index != -1)//以start_addr开始的ip已经在训练中
    {
        if (this->training_time_recorder[index].next_addr.addr == next_addr)
        {
            if (this->training_time_recorder[index].next_addr.conf * this->training_time_recorder[index].lru >= 2) {
                this->training_time_recorder.erase(training_time_recorder.begin() + index);//从训练完成里面剔除
                update_trained_time_recorder(start_addr, next_addr);//加入训练的time_finder中
                return;
            }
            if (this->training_time_recorder[index].next_addr.conf < DEFAULT_CONF)
            {
                this->training_time_recorder[index].next_addr.conf += 1;
            }
            this->training_time_recorder[index].lru ++;
        }
        else
        {
            if (this->training_time_recorder[index].next_addr.conf * this->training_time_recorder[index].lru <= -1)
            {
                this->training_time_recorder.erase(training_time_recorder.begin() + index);//从训练中里面剔除
            }
            else
            {
                this->training_time_recorder[index].next_addr.conf -= 1;
                this->training_time_recorder[index].lru ++;
            }
        }
    }
    else
    { //从来没有记录过该ip对应start_addr的下一个访问地址
        //is not full
        if (this->training_time_recorder.size() < TRAINING_ENTRY_NUM)
        {
            Next_addr nextAddr;
            nextAddr.addr = next_addr;
            nextAddr.conf = 1;
            Addr_pair addrPair;
            addrPair.start_addr = start_addr;
            addrPair.next_addr = nextAddr;
            addrPair.lru = 1;
            this->training_time_recorder.push_back(addrPair);
        }
        else
        {
            //find victim
            int victim_index = 0;
            int old_value = 2;
            for (int i = 0; i < this->training_time_recorder.size(); ++i)
            {
                if (this->training_time_recorder[index].next_addr.conf * this->training_time_recorder[index].lru < old_value)
                {
                    victim_index = i;
                    old_value = this->training_time_recorder[index].next_addr.conf * this->training_time_recorder[index].lru;
                }
            }
            training_time_recorder.erase(training_time_recorder.begin() + victim_index);
            //add new pair
            Next_addr nextAddr;
            nextAddr.addr = next_addr;
            nextAddr.conf = 1;
            Addr_pair addrPair;
            addrPair.start_addr = start_addr;
            addrPair.next_addr = nextAddr;
            addrPair.lru = 1;
            this->training_time_recorder.push_back(addrPair);
        }
    }
}

/**
 * 前提：本次访问ip一定记录过对应的历史访问信息
 * 参数：本次访问的ip 开始地址 和 下一个地址（address对应的cacheline的首地址）
 * 操作：用于更新该ip对应的 记录pattern信息的 成员
 **/
void Time_finder::update_time_recorder(uint64_t start_addr, uint64_t next_addr)
{
    int index = -1;
    for (int i = 0; i < this->trained_time_recorder.size(); ++i)
    {
        if (this->trained_time_recorder[i].start_addr == start_addr)
        {
            index = i;
            break;
        }
    }
    if(index != -1) {
        update_trained_time_recorder(start_addr, next_addr);
    } else {
        update_training_time_recorder(start_addr, next_addr);
    }
}

/**
 * 功能：时间流的训练
 * 参数：本次访问的 ip address对应cacheline的第一个地址 page等信息
 * 具体操作：只对换页情况的ip进行训练（更新此ip的pattern）
 **/
void Time_finder::train(uint64_t ip, uint64_t cache_line, uint64_t page)
{
    //need to update?(只记录换页的?)
    if (this->ip_last_addr.find(ip) != this->ip_last_addr.end())
    {
        uint64_t last_addr = this->ip_last_addr[ip];
        uint64_t last_page = last_addr >> LOG2_PAGE_SIZE;
        //如果没有换页，认为不是时间流，不进行训练
        if (last_page == page)
        {
            this->update_ip_last_addr(ip, cache_line);
            return;
        }
        //否则进行训练（即更新该ip对应的pattern）
        else
        {
            //考虑增加动态调整DEGREE的功能，所以先分开(wuhao MICRO 19)
            uint64_t pref_next_addr = this->find_next_addr(last_addr);
            //same
            if (-1 == pref_next_addr)
            { //这里应该有问题吧，如果是-1的话，直接调用updata_time_recorder会出错
                this->update_time_recorder(last_addr, cache_line);
            }
            else if (pref_next_addr == cache_line)
            {
                this->update_time_recorder(last_addr, cache_line);
            }
            else
            {
                this->update_time_recorder(last_addr, cache_line);
            }
        }
    }
    this->update_ip_last_addr(ip, cache_line);
}

int Time_finder::get_trained_time_recorder_size()
{
    int num = trained_time_recorder.size();
    return num;
}

int Time_finder::get_training_time_recorder_size()
{
    int num = training_time_recorder.size();
    return num;
}