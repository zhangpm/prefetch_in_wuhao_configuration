/*************************************************************************************************************************
Authors:
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
Nilay Shah - nilays@iitk.ac.in
Neelu Shivprakash kalani - neeluk@cse.iitk.ac.in
**************************************************************************************************************************/
/*************************************************************************************************************************
Source code for "Bouquet of Instruction Pointers: Instruction Pointer Classifier-based Spatial Hardware Prefetching"
appeared (to appear) in ISCA 2020: https://www.iscaconf.org/isca2020/program/. The paper is available at
https://www.cse.iitk.ac.in/users/biswap/IPCP_ISCA20.pdf. The source code can be used with the ChampSim simulator
https://github.com/ChampSim . Note that the authors have used a modified ChampSim that supports detailed virtual
memory sub-system. Performance numbers may increase/decrease marginally
based on the virtual memory-subsystem support. Also for PIPT L1-D caches, this code may demand 1 to 1.5KB additional
storage for various hardware tables.
**************************************************************************************************************************/



#include "ooo_cpu.h"
#include "cache.h"
#include <vector>
#include <algorithm>
#include "log.h"
#include "time_finder.h"
#include "ip_classifier.h"

using namespace std;

#define DO_PREF

#define NUM_BLOOM_ENTRIES 4096
#define NUM_IP_TABLE_L2_ENTRIES 64
#define NUM_IP_INDEX_BITS_L2 6
#define NUM_IP_TAG_BITS_L2 9
#define S_TYPE 1                                            // stream
#define CS_TYPE 2                                           // constant stride
#define CPLX_TYPE 3                                         // complex stride
#define NL_TYPE 4                                           // next line


// #define SIG_DEBUG_PRINT_L2				    //Uncomment to enable debug prints
#ifdef SIG_DEBUG_PRINT_L2
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif

class STAT_COLLECT {
  public:
    uint64_t useful;
    uint64_t filled;
    uint64_t misses;
    uint64_t polluted_misses;

    uint8_t bl_filled[NUM_BLOOM_ENTRIES];
    uint8_t bl_request[NUM_BLOOM_ENTRIES];

    STAT_COLLECT () {
        useful = 0;
        filled = 0;
        misses = 0;
        polluted_misses = 0;

        for(int i=0; i<NUM_BLOOM_ENTRIES; i++){
            bl_filled[i] = 0;
            bl_request[i] = 0;
        }
    };
};

class IP_TABLE {
  public:
    uint64_t ip_tag;						// ip tag
    uint16_t ip_valid;						// ip valid bit
    uint32_t pref_type;                                     	// prefetch class type
    int stride;							// stride or stream

    IP_TABLE () {
        ip_tag = 0;
        ip_valid = 0;
        pref_type = 0;
        stride = 0;
    };
};

/*      IP TABLE STORAGE OVERHEAD: 288 Bytes

        Single Entry:

        FIELD                                   STORAGE (bits)

        IP tag                                  9
        IP valid                                1
	stride		                        7       (6 bits stride + 1 sign bit)
        prefetch type				2

	Total                                   19

        Full Table Storage Overhead:

        64 entries * 19 bits = 1216 bits = 152 Bytes

*/


STAT_COLLECT stats_l2[NUM_CPUS][5];     // for GS, CS, CPLX, NL and no class
uint64_t num_misses_l2[NUM_CPUS] = {0};
//DELTA_PRED_TABLE CSPT_l2[NUM_CPUS][NUM_CSPT_L2_ENTRIES];
uint32_t spec_nl_l2[NUM_CPUS] = {0};
IP_TABLE trackers[NUM_CPUS][NUM_IP_TABLE_L2_ENTRIES];


uint64_t hash_bloom_l2(uint64_t addr){//对地址求hash
    uint64_t first_half, sec_half;
    first_half = addr & 0xFFF;
    sec_half = (addr >> 12) & 0xFFF;
    if((first_half ^ sec_half) >= 4096)
        assert(0);
    return ((first_half ^ sec_half) & 0xFFF);
}

/*decode_stride: This function decodes 7 bit stride from the metadata from IPCP at L1. 6 bits for magnitude and 1 bit for sign. */

int decode_stride(uint32_t metadata){//解析由l1d传过来的metadata
    int stride=0;
    if(metadata & 0b1000000)
        stride = -1*(metadata & 0b111111);
    else
        stride = metadata & 0b111111;

    return stride;
}

/* update_conf_l2: If the actual stride and predicted stride are equal, then the confidence counter is incremented. */

int update_conf_l1(int stride, int pred_stride, int conf){//更新置信度的函数，当stride相同的时候置信度增加一
    if(stride == pred_stride){             // use 2-bit saturating counter for confidence
        conf++;
        if(conf > 3)
            conf = 3;
    } else {
        conf--;
        if(conf < 0)
            conf = 0;
    }

return conf;
}

/* encode_metadata_l2: This function encodes the stride, prefetch class type and speculative nl fields in the metadata. */

uint32_t encode_metadata_l2(int stride, uint16_t type, int spec_nl_l2){//l2层的metadata压缩

    uint32_t metadata = 0;

    // first encode stride in the last 8 bits of the metadata
    if(stride > 0)
        metadata = stride;
    else
        metadata = ((-1*stride) | 0b1000000);

    // encode the type of IP in the next 4 bits
    metadata = metadata | (type << 8);

    // encode the speculative NL bit in the next 1 bit
    metadata = metadata | (spec_nl_l2 << 12);

    return metadata;

}

void stat_col_L2(uint64_t addr, uint8_t cache_hit, uint8_t cpu, uint64_t ip){
    uint64_t index = hash_bloom_l2(addr);
    int ip_index = ip & ((1 << NUM_IP_INDEX_BITS_L2)-1);
    uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS_L2) & ((1 << NUM_IP_TAG_BITS_L2)-1);

    for(int i=0; i<5; i++){
        if(cache_hit){
            if(stats_l2[cpu][i].bl_filled[index] == 1){
                stats_l2[cpu][i].useful++;
                stats_l2[cpu][i].filled++;
                stats_l2[cpu][i].bl_filled[index] = 0;
            }
        } else {
            if(ip_tag == trackers[cpu][ip_index].ip_tag){
                if(trackers[cpu][ip_index].pref_type == i)
                    stats_l2[cpu][i].misses++;
                if(stats_l2[cpu][i].bl_filled[index] == 1){
                    stats_l2[cpu][i].polluted_misses++;
                    stats_l2[cpu][i].filled++;
                    stats_l2[cpu][i].bl_filled[index] = 0;
                }
            }
        }

        if(num_misses_l2[cpu] % 1024 == 0){
            for(int j=0; j<NUM_BLOOM_ENTRIES; j++){
                stats_l2[cpu][i].filled += stats_l2[cpu][i].bl_filled[j];
                stats_l2[cpu][i].bl_filled[j] = 0;
                stats_l2[cpu][i].bl_request[j] = 0;
            }
        }
    }
}


Time_finder *time_finder = new Time_finder();


namespace mix_time_l2{
    #define TIME_DEGREE 3


    //important
    vector<uint64_t> important_ips;

    vector<uint64_t> read_important_files(string file_path)
    {
        ifstream file(file_path);
        string line;
        vector<uint64_t> line_contents;
        if (file) // 有该文件
        {
            while (getline(file, line)) // line中不包括每行的换行符
            {
                line_contents.push_back(stoi(line));
            }
        }
        else // 没有该文件
        {
            cout << "no such file" << endl;
        }
        return line_contents;
    }

    //time

    map<uint64_t, uint64_t> ip_last_addr;
    map<uint64_t, map<uint64_t, uint64_t>> ip_addr_pair;

    uint64_t find_next_addr_by_first_addr(uint64_t ip, uint64_t first_addr){
        if(ip_addr_pair[ip].find(first_addr) != ip_addr_pair[ip].end()){
            return ip_addr_pair[ip][first_addr];
        }
        return -1;
    }

    void update_ip_addr_pair(uint64_t ip, uint64_t last_addr, uint64_t addr){
        if (ip_addr_pair.find(ip) == ip_addr_pair.end()){
            ip_addr_pair[ip] = map<uint64_t, uint64_t>();
            ip_addr_pair[ip][last_addr] = addr;
        }
        else {
            ip_addr_pair[ip][last_addr] = addr;
        }
    }

    void update_ip_last_addr(uint64_t ip, uint64_t addr){
        if (ip_last_addr.find(ip) == ip_last_addr.end()){
            ip_last_addr.insert(make_pair(ip, addr));
        }
        else{
            uint64_t page = addr >> LOG2_PAGE_SIZE;
            uint64_t last_addr = ip_last_addr[ip];
            uint64_t last_page = last_addr >> LOG2_PAGE_SIZE;
            if (page == last_page){
                ip_last_addr[ip] = addr;
            }
            else{
                update_ip_addr_pair(ip, last_addr, addr);
                ip_last_addr[ip] = addr;
            }
        }
    }

    vector<uint64_t> make_prefetch_by_time(uint64_t ip, uint64_t addr){
        //make prefetch
        vector<uint64_t> pf_addresses;
        if (ip_addr_pair.find(ip) != ip_addr_pair.end()) {
            uint64_t first_addr = addr;
            for (int i = 0; i < TIME_DEGREE; ++i) {
                uint64_t pf_addr = find_next_addr_by_first_addr(ip, first_addr);
                if(pf_addr != -1){
                    //ip, addr, pf_address, FILL_L1, metadata
                    //prefetch_line_diff_page(ip, addr, pf_addr, FILL_L1, 0);
                    pf_addresses.push_back(pf_addr);
                    first_addr = pf_addr;
                }
                else{
                    break;
                }
            }
        }
        //update
        update_ip_last_addr(ip, addr);
        return pf_addresses;
    }

    //ip classifier
    Ip_classifier *ipClassifier_l2 = new Ip_classifier();
}

using namespace mix_time_l2;

void CACHE::l2c_prefetcher_initialize()
{
    important_ips = read_important_files("important_ips.txt");
}


uint64_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint64_t metadata_in)
{
    //time_ip
    ipClassifier_l2->update(ip, addr);
    vector<uint64_t> important_ips_l2 = ipClassifier_l2->get_important_ips();
    //如果是时间特征的ip
    if(important_ips_l2.end() != find(important_ips_l2.begin(), important_ips_l2.end(), ip)){//这里应该是找到了吧？
        uint64_t cache_line = (addr >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE;
        uint64_t page = addr >> LOG2_PAGE_SIZE;
        vector<uint64_t> pf_addresses = time_finder->predict(ip, cache_line);
        time_finder->train(ip, cache_line, page, ipClassifier_l2->get_erase_time_ip());
        //vector<uint64_t>pf_addresses = make_prefetch_by_time(ip, (addr >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);

        for (int i = 0; i < pf_addresses.size(); ++i) {
            prefetch_line(ip, addr, pf_addresses[i], FILL_L2, 0);
        }
    }

    uint64_t page = addr >> LOG2_PAGE_SIZE;
    //uint64_t curr_tag = (page ^ (page >> 6) ^ (page >> 12)) & ((1<<NUM_IP_TAG_BITS_L2)-1);
    uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;		//cache line address
    uint64_t line_offset = (addr >> LOG2_BLOCK_SIZE) & 0x3F; 	//cache line offset，即line_offset的范围是0~127
    int prefetch_degree = 0;
    int64_t stride = decode_stride(metadata_in);
    uint32_t pref_type = (metadata_in & 0xF00) >> 8;
    uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS_L2) & ((1 << NUM_IP_TAG_BITS_L2)-1);
    int num_prefs = 0;
    uint64_t bl_index = 0;
    if(NUM_CPUS == 1){
        prefetch_degree = 3;
    } else {                                    // tightening the degree for multi-core
        prefetch_degree = 2;
    }

    stat_col_L2(addr, cache_hit, cpu, ip);
    if(cache_hit == 0 && type != PREFETCH)
        num_misses_l2[cpu]++;

    // calculate the index bit
    int index = ip & ((1 << NUM_IP_INDEX_BITS_L2)-1);
    if(trackers[cpu][index].ip_tag != ip_tag){              // new/conflict IP
        if(trackers[cpu][index].ip_valid == 0){             // if valid bit is zero, update with latest IP info
            trackers[cpu][index].ip_tag = ip_tag;
            trackers[cpu][index].pref_type = pref_type;
            trackers[cpu][index].stride = stride;
        } else {
            trackers[cpu][index].ip_valid = 0;                  // otherwise, reset valid bit and leave the previous IP as it is
        }

        // issue a next line prefetch upon encountering new IP
        uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
        //Ensure it lies on the same 4 KB page.
        if ((pf_address >> LOG2_PAGE_SIZE) == (addr >> LOG2_PAGE_SIZE))
        {
                #ifdef DO_PREF
                prefetch_line(ip, addr, pf_address, FILL_L2, 0);
                #endif
                SIG_DP(cout << "1, ");
        }
        return metadata_in;
    }
    else {                                                  // if same IP encountered, set valid bit
        trackers[cpu][index].ip_valid = 1;
    }

    // update the IP table upon receiving metadata from prefetch
    if(type == PREFETCH){
        trackers[cpu][index].pref_type = pref_type;
        trackers[cpu][index].stride = stride;
        spec_nl_l2[cpu] = metadata_in & 0x1000;
    }

    SIG_DP(
    cout << ip << ", " << cache_hit << ", " << line_addr << ", ";
    cout << ", " << stride << "; ";
    );


    if((trackers[cpu][index].pref_type == 1 || trackers[cpu][index].pref_type == 2) && trackers[cpu][index].stride != 0){      // S or CS class
        uint32_t metadata = 0;
        if(trackers[cpu][index].pref_type == 1){
            prefetch_degree = prefetch_degree*2;
            metadata = encode_metadata_l2(1, S_TYPE, spec_nl_l2[cpu]);                               // for stream, prefetch with twice the usual degree
        } else{
            metadata = encode_metadata_l2(1, CS_TYPE, spec_nl_l2[cpu]);
        }

        for (int i=0; i<prefetch_degree; i++) {
            uint64_t pf_address = (line_addr + (trackers[cpu][index].stride*(i+1))) << LOG2_BLOCK_SIZE;

            // Check if prefetch address is in same 4 KB page
            if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                break;
            num_prefs++;
            #ifdef DO_PREF
            prefetch_line(ip, addr, pf_address, FILL_L2, metadata);
            #endif
            SIG_DP(cout << trackers[cpu][index].stride << ", ");
        }
    }

    // if no prefetches are issued till now, speculatively issue a next_line prefetch
    if(num_prefs == 0 && spec_nl_l2[cpu] == 1){                                        // NL IP
        uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
        //If it is not in the same 4 KB page, return.
        if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
        {
            return metadata_in;
        }
        bl_index = hash_bloom_l2(pf_address);
        stats_l2[cpu][NL_TYPE].bl_request[bl_index] = 1;
        uint32_t metadata = encode_metadata_l2(1, NL_TYPE, spec_nl_l2[cpu]);
        trackers[cpu][index].pref_type = 3;
        #ifdef DO_PREF
        prefetch_line(ip, addr, pf_address, FILL_L2, metadata);
        #endif
        SIG_DP(cout << "1, ");
    }
    SIG_DP(cout << endl);
    return metadata_in;
}

uint64_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in)
{

    if(prefetch){
        uint32_t pref_type = metadata_in & 0xF00;
        pref_type = pref_type >> 8;

        uint64_t index = hash_bloom_l2(addr);
        if(stats_l2[cpu][pref_type].bl_request[index] == 1){
            stats_l2[cpu][pref_type].bl_filled[index] = 1;
            stats_l2[cpu][pref_type].bl_request[index] = 0;
        }
    }
    return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{

}

void CACHE::complete_metadata_req(uint64_t meta_data_addr)
{
}