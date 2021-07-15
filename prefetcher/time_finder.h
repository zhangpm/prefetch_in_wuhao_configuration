//
// Created by Umasou on 2021/6/23.
//

#ifndef CHAMPSIMSERVER_TIME_FINDER_H
#define CHAMPSIMSERVER_TIME_FINDER_H

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

//< 50  106 40%   ip:5  entry:20    <500
#define TRAINING_ENTRY_NUM 300
#define TRAINED_ENTRY_NUM 300
//改成动态调整的形式
#define PREFETCH_DEGREE 1
#define DEFAULT_CONF 3

struct Next_addr{
    uint64_t addr;
    int conf;
};

struct Addr_pair{
    uint64_t start_addr;
    Next_addr next_addr;
    int lru;
};

class Time_finder{

public:
    Time_finder(){};
    void train(uint64_t ip, uint64_t cache_line, uint64_t page);
    vector<uint64_t> predict(uint64_t cache_line);
    int get_trained_time_recorder_size();
    int get_training_time_recorder_size();
    void repl_ip(vector<uint64_t> erase_ips);

private:
    void update_time_recorder(uint64_t start_addr, uint64_t next_addr);
    void update_training_time_recorder(uint64_t start_addr, uint64_t next_addr);
    void update_trained_time_recorder(uint64_t start_addr, uint64_t next_addr);
    void update_ip_last_addr(uint64_t ip, uint64_t addr);
    uint64_t find_next_addr(uint64_t addr);
    //置换策略需要修改，参考wuhao
    vector<Addr_pair> training_time_recorder;
    vector<Addr_pair> trained_time_recorder;
    //ip, last cache line
    map<uint64_t, uint64_t> ip_last_addr;

};

#endif //CHAMPSIMSERVER_TIME_FINDER_H
