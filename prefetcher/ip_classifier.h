//
// Created by Umasou on 2021/6/24.
//

#ifndef CHAMPSIMSERVER_IP_CLASSIFIER_H
#define CHAMPSIMSERVER_IP_CLASSIFIER_H

#include <map>
#include <iostream>
#include <vector>

#define FINISH_THRESHOLD 3
#define IMPORTANT_IPS_SIZE 20
#define IP_NUM 64 //用于训练学习的ip个数
#define VICTIM_THRESHOLD -3
using namespace std;

class Ip_classifier{

public:
    void update(uint64_t ip, uint64_t addr);

    vector<uint64_t> get_important_ips();

    vector<uint64_t> get_erase_time_ip(){return this->erase_ips;}

private:
    void update_when_ip_is_full();

    void update_important_ips(uint64_t ip);

    map<uint64_t, pair<uint64_t, uint64_t>> ip_jump;//该ip换页的次数&总的次数
    map<uint64_t, uint64_t> ip_last_cl;//记录ip上次访问的页面
    map<uint64_t, uint64_t> important_ip_map;//记录当前学习到的具有时间特征的ip集合，value为lru计数器
    vector<uint64_t> erase_ips;
};

#endif //CHAMPSIMSERVER_IP_CLASSIFIER_H
