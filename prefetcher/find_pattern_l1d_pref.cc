//
// Created by Umasou on 2021/6/22.
//
#include "cache.h"
#include <map>
#include <algorithm>
#include <fstream>
#include <vector>
#include "log.h"

using namespace std;

vector<uint64_t> important_ips = {};
map<uint64_t, vector<uint64_t>> ip_pattern;
ChampSimLog cslog("target_ip_pattern.txt");
ChampSimLog cslog1("target_mix_pattern.txt");
uint64_t total_pattern_num;


void CACHE::l1d_prefetcher_initialize()
{
    total_pattern_num = 0;
    ifstream infile;
    infile.open("important_ips.txt", ios::in);
    if(!infile.is_open ())
        cout << "Open file failure" << endl;
    while (!infile.eof())         // 若未到文件结束一直循环
    {
        uint64_t temp;
        infile >> temp;
        important_ips.push_back(temp);
        cout << temp;     
    }
    infile.close();   //关闭文件
}

void CACHE::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    total_pattern_num++;
    if(important_ips.end() == find(important_ips.begin(), important_ips.end(), ip)){
         if(ip_pattern.find(ip) == ip_pattern.end()){
             ip_pattern[ip] = vector<uint64_t>();
         }
         ip_pattern[ip].push_back(addr);
    }
    cslog1.makeLog(string("ip:" + to_string(ip) + " page:" + to_string(addr >> LOG2_PAGE_SIZE) + " offset:" + to_string((addr >> LOG2_BLOCK_SIZE) & 0X3F )), true);
}

void CACHE::l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in)
{

}

void CACHE::l1d_prefetcher_final_stats()
{
    cslog.makeLog("total_pattern_num:"+to_string(total_pattern_num), true);
    for (auto ip_itor = ip_pattern.begin(); ip_itor != ip_pattern.end() ; ip_itor++) {
        cslog.makeLog(string("ip :　" + to_string(ip_itor->first)), true);
        cout << ip_itor->first <<endl;
        for (auto addr_itor = ip_itor->second.begin(); addr_itor != ip_itor->second.end() ; addr_itor++) {
            uint64_t addr = *addr_itor;
            uint64_t curr_page = addr >> LOG2_PAGE_SIZE; 	//current page
            uint64_t line_offset = (addr >> LOG2_BLOCK_SIZE) & 0x3F; 	//cache line offset
            cout << "addr:" << to_string(addr) << "page:" << to_string(curr_page) << "offset:" << to_string(line_offset) << endl;
            cslog.makeLog(string("addr : " + to_string(addr) + " page : " + to_string(curr_page) + " offset : " + to_string(line_offset)), true);

        }
        cslog.makeLog(("**************"), true);
    }
}

