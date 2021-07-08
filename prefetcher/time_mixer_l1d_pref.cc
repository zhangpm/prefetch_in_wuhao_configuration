/*************************************************************************************************************************
Authors:
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
Nilay Shah - nilays@iitk.ac.in
Neelu Shivprakash kalani - neeluk@cse.iitk.ac.in
**************************************************************************************************************************/

/*************************************************************************************************************************
Source code of "Bouquet of Instruction Pointers: Instruction Pointer Classifier-based Spatial Hardware Prefetching"
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
#include "ip_classifier.h"

using namespace std;

/**************************************************************************************************************************
Note that variables uint64_t pref_useful[NUM_CPUS][6], pref_filled[NUM_CPUS][6], pref_late[NUM_CPUS][6]; are to be declared
as members of the CACHE class in inc/cache.h and modified in src/cache.cc where the second level index denotes the IPCP prefetch
class type for each variable which can be extracted through pf_metadata. A prefetch is considered in pref_useful when a cache
blocks gets a hit and its prefetch bit is set. Whenever a cache block is filled (in handle_fill) and its type is prefetch,
pref_fill is incremented. The pref_late variable is modified whenever a demand request merges with a prefetch request or
vice versa in the cache's MSHR as, if the prefetch would've been on time, the demand request would've hit in the cache.
****************************************************************************************************************************/


#define DO_PREF
#define NUM_BLOOM_ENTRIES 4096				    // For book-keeping purposes
#define NUM_IP_TABLE_L1_ENTRIES 64
#define NUM_CSPT_ENTRIES 128			   	    // = 2^NUM_SIG_BITS
#define NUM_SIG_BITS 7					    // num of bits in signature
#define NUM_IP_INDEX_BITS 6
#define NUM_IP_TAG_BITS 9
#define NUM_PAGE_TAG_BITS 2
#define S_TYPE 1                                            // stream
#define CS_TYPE 2                                           // constant stride
#define CPLX_TYPE 3                                         // complex stride
#define NL_TYPE 4                                           // next line
#define CPLX_DIST 0
#define NUM_OF_RR_ENTRIES 32				    // recent request filter entries
#define RR_TAG_MASK 0xFFF				    // 12 bits of prefetch line address are stored in recent request filter
#define NUM_RST_ENTRIES 8                                   // region stream table entries
#define MAX_POS_NEG_COUNT 64				    // 6-bit saturating counter
#define NUM_OF_LINES_IN_REGION 32			    // 32 cache lines in 2KB region
#define REGION_OFFSET_MASK 0x1F				    // 5-bit offset for 2KB region

// #define SIG_DEBUG_PRINT				    // Uncomment to turn on Debug Print
#ifdef SIG_DEBUG_PRINT
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif

class IP_TABLE_L1 {
public:
    uint64_t ip_tag;
    uint64_t last_vpage;                                    // last page seen by IP
    uint64_t last_line_offset;                              // last cl offset in the 4KB page
    int64_t last_stride;                                    // last stride observed
    uint16_t ip_valid;					                    // valid bit
    int conf;                                               // CS confidence
    uint16_t signature;                                     // CPLX signature
    uint16_t str_dir;                                       // stream direction
    uint16_t str_valid;                                     // stream valid
    uint16_t pref_type;                                     // pref type or class for book-keeping purposes.

    IP_TABLE_L1 () {
        ip_tag = 0;
        last_vpage = 0;
        last_line_offset = 0;
        last_stride = 0;
        ip_valid = 0;
        signature = 0;
        conf = 0;
        str_dir = 0;
        str_valid = 0;
        pref_type = 0;
    };
};

/*	IP TABLE STORAGE OVERHEAD: 288 Bytes

	Single Entry:

	FIELD					STORAGE (bits)

	IP tag					9
	last page				2
	last line offset			6
	last stride				7 	(6 bits stride + 1 sign bit)
	IP valid				1
	confidence				2
	signature				7
	stream direction			1	1
	stream valid				1

	Total 					36

	Full Table Storage Overhead:

	64 entries * 36 bits = 2304 bits = 288 Bytes

	NOTE: The field prefetch class is used for book-keeping purposes.

*/


class CONST_STRIDE_PRED_TABLE {
public:
    int stride;
    int conf;

    CONST_STRIDE_PRED_TABLE () {
        stride = 0;
        conf = 0;
    };
};

/*	CONSTANT STRIDE STORAGE OVERHEAD: 144 Bytes

	Single Entry:

	FIELD					STORAGE (bits)

	stride					7	(6 bits stride + 1 sign bit)
	confidence 				2

	Total					9

	Full Table Storage Overhead:

	128 entries * 9 bits = 1152 bits = 144 Bytes

*/

/* This class is for bookkeeping purposes only. */
class STAT_COLLECT {
public:
    uint64_t useful;
    uint64_t filled;
    uint64_t misses;
    uint64_t polluted_misses;

    uint8_t bl_filled[NUM_BLOOM_ENTRIES];//用hash后的address做索引
    uint8_t bl_request[NUM_BLOOM_ENTRIES];//用hash后的address做索引

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

class REGION_STREAM_TABLE {
public:
    uint64_t region_id;
    uint64_t tentative_dense;			// tentative dense bit
    uint64_t trained_dense;				// trained dense bit
    uint64_t pos_neg_count;				// positive/negative stream counter
    uint64_t dir;					// direction of stream - 1 for +ve and 0 for -ve
    uint64_t lru;					// lru for replacement
    uint8_t line_access[NUM_OF_LINES_IN_REGION];	// bit vector to store which lines in the 2KB region have been accessed
    REGION_STREAM_TABLE () {
        region_id=0;
        tentative_dense=0;
        trained_dense=0;
        pos_neg_count=MAX_POS_NEG_COUNT/2;
        dir=0;
        lru=0;
        for(int i=0; i < NUM_OF_LINES_IN_REGION; i++)
            line_access[i]=0;
    };
};

/*	REGION STREAM TABLE STORAGE OVERHEAD:

	Single Entry:

	FIELD					STORAGE (bits)

	region id				3
	tentative dense				1
	trained dense				1
	positive/negative count			6
	direction				1
	lru 					3
	bit vector line access			32	(for 2KB region)

	Total					47

	Full Table Storage Overhead:

	8 entries * 47 bits = 376 bits = 47 Bytes

*/


REGION_STREAM_TABLE rstable [NUM_CPUS][NUM_RST_ENTRIES];
int acc_filled[NUM_CPUS][5];
int acc_useful[NUM_CPUS][5];

int acc[NUM_CPUS][5];
int prefetch_degree[NUM_CPUS][5];
int num_conflicts =0;//记录ip冲突的数量（trakers_l1大小有限）
int test;

uint64_t eval_buffer[NUM_CPUS][1024] = {};
STAT_COLLECT stats[NUM_CPUS][5];     // for GS, CS, CPLX, NL and no class
IP_TABLE_L1 trackers_l1[NUM_CPUS][NUM_IP_TABLE_L1_ENTRIES];//用ip的0~5位作为索引
CONST_STRIDE_PRED_TABLE CSPT_l1[NUM_CPUS][NUM_CSPT_ENTRIES];

vector<uint64_t> recent_request_filter;		// to filter redundant prefetch requests

/* 	RECENT REQUEST FILTER STORAGE OVERHEAD: 48 Bytes

	FIELD					STORAGE (bits)

	Tag					12

	Total Storage Overhead:

	32 entries * 12 bits = 384 bits = 48 Bytes

*/

uint64_t prev_cpu_cycle[NUM_CPUS];
uint64_t num_misses[NUM_CPUS];//warm_up完成之后 未命中的个数
float mpki[NUM_CPUS] = {0};
int spec_nl[NUM_CPUS] = {0}, flag_nl[NUM_CPUS] = {0};//flag_nl标识是否进行next line预取？
uint64_t num_access[NUM_CPUS];//每调用一次operator函数，值就加一

int meta_counter[NUM_CPUS][4] = {0};                                                  // for book-keeping
int total_count[NUM_CPUS] = {0};                                                  // for book-keeping


/* update_sig_l1: 7 bit signature is updated by performing a left-shift of 1 bit on the old signature and xoring the outcome with the delta*/

uint16_t update_sig_l1(uint16_t old_sig, int delta){
    uint16_t new_sig = 0;
    int sig_delta = 0;

    // 7-bit sign magnitude form, since we need to track deltas from +63 to -63
    sig_delta = (delta < 0) ? (((-1) * delta) + (1 << 6)) : delta;
    new_sig = ((old_sig << 1) ^ sig_delta) & ((1 << NUM_SIG_BITS)-1);

    return new_sig;
}

/* encode_metadata: The stride, prefetch class type and speculative nl fields are encoded in the metadata. */

uint32_t encode_metadata(int stride, uint16_t type, int spec_nl){
    uint32_t metadata = 0;

    // first encode stride in the last 8 bits of the metadata
    if(stride > 0)
        metadata = stride;
    else
        metadata = ((-1*stride) | 0b1000000);

    // encode the type of IP in the next 4 bits
    metadata = metadata | (type << 8);

    // encode the speculative NL bit in the next 1 bit
    metadata = metadata | (spec_nl << 12);

    return metadata;
}

/*If the actual stride and predicted stride are equal, then the confidence counter is incremented. */

int update_conf(int stride, int pred_stride, int conf){
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


uint64_t hash_bloom(uint64_t addr){//对address哈希后结果不大于4096
    uint64_t first_half, sec_half;
    first_half = addr & 0xFFF;
    sec_half = (addr >> 12) & 0xFFF;
    if((first_half ^ sec_half) >= 4096)
        assert(0);
    return ((first_half ^ sec_half) & 0xFFF);
}

uint64_t hash_page(uint64_t addr){//对page做hash，传入的参数是页号，不是物理地址
    uint64_t hash;
    while(addr != 0){
        hash = hash ^ addr;
        addr = addr >> 6;
    }

    return hash & ((1 << NUM_PAGE_TAG_BITS)-1);
}

/**
 * 功能：更新状态统计信息
 * 问题：暂时没太看懂
 **/
void stat_col_L1(uint64_t addr, uint8_t cache_hit, uint8_t cpu, uint64_t ip){
    uint64_t index = hash_bloom(addr);
    int ip_index = ip & ((1 << NUM_IP_INDEX_BITS)-1);//索引是0~5位
    uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);//tag是6~14位


    for(int i=0; i<5; i++){
        if(cache_hit){
            if(stats[cpu][i].bl_filled[index] == 1){
                stats[cpu][i].useful++;
                stats[cpu][i].filled++;
                stats[cpu][i].bl_filled[index] = 0;
            }
        } else {
            if(ip_tag == trackers_l1[cpu][ip_index].ip_tag){
                if(trackers_l1[cpu][ip_index].pref_type == i)
                    stats[cpu][i].misses++;
                if(stats[cpu][i].bl_filled[index] == 1){
                    stats[cpu][i].polluted_misses++;
                    stats[cpu][i].filled++;
                    stats[cpu][i].bl_filled[index] = 0;
                }
            }
        }

        if(num_misses[cpu] % 1024 == 0){
            for(int j=0; j<NUM_BLOOM_ENTRIES; j++){
                stats[cpu][i].filled += stats[cpu][i].bl_filled[j];
                stats[cpu][i].bl_filled[j] = 0;
                stats[cpu][i].bl_request[j] = 0;
            }
        }
    }
}

namespace mix_time_l1{
#define TIME_DEGREE 3

    //important
    vector<uint64_t>important_ips;

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
        if (ip_addr_pair.find(ip) != ip_addr_pair.end()){
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
    Ip_classifier *ipClassifier_l1 = new Ip_classifier();
}



using namespace mix_time_l1;



void CACHE::l1d_prefetcher_initialize()
{
    for( int i=0; i <NUM_RST_ENTRIES; i++)
        rstable[cpu][i].lru = i;
    for( int i=0; i <NUM_CPUS; i++)
    {
        prefetch_degree[cpu][0] = 0;
        prefetch_degree[cpu][1] = 6;
        prefetch_degree[cpu][2] = 3;
        prefetch_degree[cpu][3] = 3;
        prefetch_degree[cpu][4] = 1;

    }
}

void CACHE::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    //find important ips
    ipClassifier_l1->update(ip, addr);
    vector<uint64_t> important_ips_l1 = ipClassifier_l1->get_important_ips();
    //time prefetch
    //is ip important
   // if(important_ips_l1.size() > 0) {
   // 	cout << "时间特征ip集合不为0\n";
   // }
    if(important_ips_l1.end() != find(important_ips_l1.begin(), important_ips_l1.end(), ip)){
        return;
    }
    else{
        uint64_t curr_page = hash_page(addr >> LOG2_PAGE_SIZE); 	//current page
        uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;		//cache line address
        uint64_t line_offset = (addr >> LOG2_BLOCK_SIZE) & 0x3F; 	//cache line offset，即line_offset的范围是0~127
        uint16_t signature = 0, last_signature = 0;
        int spec_nl_threshold = 0;
        int num_prefs = 0;//统计预取的次数
        uint32_t metadata=0;
        uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);
        uint64_t bl_index = 0;

        if(NUM_CPUS == 1){
            spec_nl_threshold = 50;
        }
        else {                                    //tightening the mpki constraints for multi-core
            spec_nl_threshold = 40;
        }

        // update miss counter
        if(cache_hit == 0 && warmup_complete[cpu] == 1)
            num_misses[cpu] += 1;
        num_access[cpu] += 1;
        stat_col_L1(addr, cache_hit, cpu, ip);
        // update spec nl bit when num misses crosses certain threshold
        if(num_misses[cpu] % 256 == 0 && cache_hit == 0){
            mpki[cpu] = ((num_misses[cpu]*1000.0)/(ooo_cpu[cpu].num_retired - ooo_cpu[cpu].warmup_instructions));

            if(mpki[cpu] > spec_nl_threshold)
                spec_nl[cpu] = 0;
            else
                spec_nl[cpu] = 1;
        }

        //Updating prefetch degree based on accuracy，动态地调整预取深度
        for(int i=0;i<5;i++)
        {
            if(pref_filled[cpu][i]%256 == 0)
            {

                acc_useful[cpu][i]=acc_useful[cpu][i]/2.0 + (pref_useful[cpu][i] - acc_useful[cpu][i])/2.0;
                acc_filled[cpu][i]=acc_filled[cpu][i]/2.0 + (pref_filled[cpu][i] - acc_filled[cpu][i])/2.0;

                if(acc_filled[cpu][i] != 0)
                    acc[cpu][i]=100.0*acc_useful[cpu][i]/(acc_filled[cpu][i]);
                else
                    acc[cpu][i] = 60;

                if(acc[cpu][i] > 75)
                {
                    prefetch_degree[cpu][i]++;
                    if(i == 1)
                    {
                        //For GS class, degree is incremented/decremented by 2.
                        prefetch_degree[cpu][i]++;
                        if(prefetch_degree[cpu][i] > 6)
                            prefetch_degree[cpu][i] = 6;
                    }
                    else if(prefetch_degree[cpu][i] > 3)
                        prefetch_degree[cpu][i] = 3;
                }
                else if(acc[cpu][i] < 40)
                {
                    prefetch_degree[cpu][i]--;
                    if(i == 1)
                        prefetch_degree[cpu][i]--;
                    if(prefetch_degree[cpu][i] < 1)
                        prefetch_degree[cpu][i] = 1;
                }

            }
        }

        // calculate the index bit
        int index = ip & ((1 << NUM_IP_INDEX_BITS)-1);
        if(trackers_l1[cpu][index].ip_tag != ip_tag) {               // new/conflict IP
            if(trackers_l1[cpu][index].ip_valid == 0){              // if valid bit is zero, update with latest IP info
                num_conflicts++;
                trackers_l1[cpu][index].ip_tag = ip_tag;
                trackers_l1[cpu][index].last_vpage = curr_page;
                trackers_l1[cpu][index].last_line_offset = line_offset;
                trackers_l1[cpu][index].last_stride = 0;
                trackers_l1[cpu][index].signature = 0;
                trackers_l1[cpu][index].conf = 0;
                trackers_l1[cpu][index].str_valid = 0;
                trackers_l1[cpu][index].str_dir = 0;
                trackers_l1[cpu][index].pref_type = 0;
                trackers_l1[cpu][index].ip_valid = 1;
            }
            else {                                                    // otherwise, reset valid bit and leave the previous IP as it is
                trackers_l1[cpu][index].ip_valid = 0;
            }
            return;
        }
        else {                                                     // if same IP encountered, set valid bit
            trackers_l1[cpu][index].ip_valid = 1;
        }


        // calculate the stride between the current cache line offset and the last cache line offset
        int64_t stride = 0;
        if (line_offset > trackers_l1[cpu][index].last_line_offset)
            stride = line_offset - trackers_l1[cpu][index].last_line_offset;
        else {
            stride = trackers_l1[cpu][index].last_line_offset - line_offset;
            stride *= -1;
        }

        // don't do anything if same address is seen twice in a row
        if (stride == 0)
            return;

        int c=0, flag = 0;//flag记录这个ip是否已经被判断为GS流

        //Checking if IP is already classified as a part of the GS class, so that for the new region we will set the tentative (spec_dense) bit.
        for(int i = 0; i < NUM_RST_ENTRIES; i++)
        {
            if(rstable[cpu][i].region_id == ((trackers_l1[cpu][index].last_vpage << 1) | (trackers_l1[cpu][index].last_line_offset >> 5)))
            {
                if(rstable[cpu][i].trained_dense == 1)
                    flag = 1;
                break;
            }
        }
        for(c=0; c < NUM_RST_ENTRIES; c++)
        {
            if(((curr_page << 1) | (line_offset >> 5)) == rstable[cpu][c].region_id)
            {
                if(rstable[cpu][c].line_access[line_offset & REGION_OFFSET_MASK] == 0)
                {
                    rstable[cpu][c].line_access[line_offset & REGION_OFFSET_MASK] = 1;
                }

                if(rstable[cpu][c].pos_neg_count >= MAX_POS_NEG_COUNT || rstable[cpu][c].pos_neg_count <= 0)
                {
                    rstable[cpu][c].pos_neg_count = MAX_POS_NEG_COUNT/2;
                }

                if(stride>0)
                    rstable[cpu][c].pos_neg_count++;
                else
                    rstable[cpu][c].pos_neg_count--;

                if(rstable[cpu][c].trained_dense == 0)
                {
                    int count = 0;
                    for(int i = 0; i < NUM_OF_LINES_IN_REGION; i++)
                        if(rstable[cpu][c].line_access[i] == 1)//line_offset & REGION_OFFSET_MASK
                            count++;

                    if(count > 24)	//75% of the cache lines in the region are accessed.
                    {
                        rstable[cpu][c].trained_dense = 1;
                    }
                }
                if (flag == 1)
                    rstable[cpu][c].tentative_dense = 1;
                if(rstable[cpu][c].tentative_dense == 1 || rstable[cpu][c].trained_dense == 1)
                {
                    if(rstable[cpu][c].pos_neg_count > (MAX_POS_NEG_COUNT/2))
                        rstable[cpu][c].dir = 1;	//1 for positive direction
                    else
                        rstable[cpu][c].dir = 0;	//0 for negative direction

                    trackers_l1[cpu][index].str_valid = 1;
                    trackers_l1[cpu][index].str_dir = rstable[cpu][c].dir;
                }
                else
                    trackers_l1[cpu][index].str_valid = 0;

                break;
            }
        }
        //curr page has no entry in rstable. Then replace lru.
        if(c== NUM_RST_ENTRIES)
        {
            //check lru
            for( c=0;c<NUM_RST_ENTRIES;c++)
            {
                if(rstable[cpu][c].lru == (NUM_RST_ENTRIES-1))
                    break;
            }
            for (int i=0; i<NUM_RST_ENTRIES; i++) {
                if (rstable[cpu][i].lru < rstable[cpu][c].lru)
                    rstable[cpu][i].lru++;
            }
            if (flag == 1)
                rstable[cpu][c].tentative_dense = 1;
            else
                rstable[cpu][c].tentative_dense = 0;

            rstable[cpu][c].region_id = (curr_page << 1) | (line_offset >> 5);//region_id的计算方法
            rstable[cpu][c].trained_dense = 0;
            rstable[cpu][c].pos_neg_count = MAX_POS_NEG_COUNT/2;
            rstable[cpu][c].dir = 0;
            rstable[cpu][c].lru = 0;
            for(int i=0; i < NUM_OF_LINES_IN_REGION; i++)
                rstable[cpu][c].line_access[i]=0;
        }



        // page boundary learning
        if(curr_page != trackers_l1[cpu][index].last_vpage){
            test++;
            if(stride < 0)
                stride += NUM_OF_LINES_IN_REGION;
            else
                stride -= NUM_OF_LINES_IN_REGION;
        }


        // update constant stride(CS) confidence
        trackers_l1[cpu][index].conf = update_conf(stride, trackers_l1[cpu][index].last_stride, trackers_l1[cpu][index].conf);

        // update CS only if confidence is zero
        if(trackers_l1[cpu][index].conf == 0)
            trackers_l1[cpu][index].last_stride = stride;

        last_signature = trackers_l1[cpu][index].signature;
        // update complex stride(CPLX) confidence
        CSPT_l1[cpu][last_signature].conf = update_conf(stride, CSPT_l1[cpu][last_signature].stride, CSPT_l1[cpu][last_signature].conf);

        // update CPLX only if confidence is zero
        if(CSPT_l1[cpu][last_signature].conf == 0)
            CSPT_l1[cpu][last_signature].stride = stride;

        // calculate and update new signature in IP table
        signature = update_sig_l1(last_signature, stride);
        trackers_l1[cpu][index].signature = signature;



        SIG_DP(
                cout << ip << ", " << cache_hit << ", " << line_addr << ", " << addr << ", " << stride << "; ";
                cout << last_signature<< ", "  << CSPT_l1[cpu][last_signature].stride<< ", "  << CSPT_l1[cpu][last_signature].conf << "; ";
                cout << trackers_l1[cpu][index].last_stride << ", " << stride << ", " << trackers_l1[cpu][index].conf << ", " << "; ";
        );
        //这里是否应该提前把flag置为0呢？
        if(trackers_l1[cpu][index].str_valid == 1){                         // stream IP
            // for stream, prefetch with twice the usual degree
            if(prefetch_degree[cpu][1] < 3)
                flag = 1;
            meta_counter[cpu][0]++;//记录进入各种流的数量，0~3分别是不同类别的流
            total_count[cpu]++;
            for (int i=0; i<prefetch_degree[cpu][1]; i++) {
                uint64_t pf_address = 0;

                if(trackers_l1[cpu][index].str_dir == 1){                   // +ve stream
                    pf_address = (line_addr + i + 1) << LOG2_BLOCK_SIZE;
                    metadata = encode_metadata(1, S_TYPE, spec_nl[cpu]);    // stride is 1
                }
                else{                                                       // -ve stream
                    pf_address = (line_addr - i - 1) << LOG2_BLOCK_SIZE;
                    metadata = encode_metadata(-1, S_TYPE, spec_nl[cpu]);   // stride is -1
                }

                if(acc[cpu][1] < 75)
                    metadata = encode_metadata(0, S_TYPE, spec_nl[cpu]);
                // Check if prefetch address is in same 4 KB page
                if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)){//跨页了就停止预取
                    break;
                }

                trackers_l1[cpu][index].pref_type = S_TYPE;

#ifdef DO_PREF
                int found_in_filter = 0;
                for(int i = 0; i < recent_request_filter.size(); i++)
                {
                    if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                    {
                        // Prefetch address is present in RR filter
                        found_in_filter = 1;
                    }
                }
                //Issue prefetch request only if prefetch address is not present in RR filter
                if(found_in_filter == 0)
                {
                    prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                    //Add to RR filter
                    recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                    if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                        recent_request_filter.erase(recent_request_filter.begin());
                }
#endif
                num_prefs++;
                SIG_DP(cout << "1, ");
            }
        } else
            flag = 1;


        if(trackers_l1[cpu][index].conf > 1 && trackers_l1[cpu][index].last_stride != 0 && flag == 1){            // CS IP
            meta_counter[cpu][1]++;
            total_count[cpu]++;

            if(prefetch_degree[cpu][2] < 2)
                flag = 1;
            else
                flag = 0;

            for (int i=0; i<prefetch_degree[cpu][2]; i++) {
                uint64_t pf_address = (line_addr + (trackers_l1[cpu][index].last_stride*(i+1))) << LOG2_BLOCK_SIZE;

                // Check if prefetch address is in same 4 KB page
                if ((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)){//跨页就停止预取
                    break;
                }

                trackers_l1[cpu][index].pref_type = CS_TYPE;
                bl_index = hash_bloom(pf_address);
                stats[cpu][CS_TYPE].bl_request[bl_index] = 1;
                if(acc[cpu][2] > 75)//只有准确率大于阈值，才告诉l2？
                    metadata = encode_metadata(trackers_l1[cpu][index].last_stride, CS_TYPE, spec_nl[cpu]);
                else
                    metadata = encode_metadata(0, CS_TYPE, spec_nl[cpu]);
                // if(spec_nl[cpu] == 1)
#ifdef DO_PREF
                int found_in_filter = 0;
                for(int i = 0; i < recent_request_filter.size(); i++)
                {
                    if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                    {
                        // Prefetch address is present in RR filter
                        found_in_filter = 1;
                    }
                }
                //Issue prefetch request only if prefetch address is not present in RR filter
                if(found_in_filter == 0)
                {
                    prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                    //Add to RR filter
                    recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                    if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                        recent_request_filter.erase(recent_request_filter.begin());
                }
#endif
                num_prefs++;
                SIG_DP(cout << trackers_l1[cpu][index].last_stride << ", ");
            }
        } else
            flag = 1;

        if(CSPT_l1[cpu][signature].conf >= 0 && CSPT_l1[cpu][signature].stride != 0 && flag == 1) {  // if conf>=0, continue looking for stride
            int pref_offset = 0,i=0;                                                        // CPLX IP
            meta_counter[cpu][2]++;
            total_count[cpu]++;

            for (i=0; i<prefetch_degree[cpu][3] + CPLX_DIST; i++) {
                pref_offset += CSPT_l1[cpu][signature].stride;
                uint64_t pf_address = ((line_addr + pref_offset) << LOG2_BLOCK_SIZE);

                // Check if prefetch address is in same 4 KB page
                if (((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE)) ||
                    (CSPT_l1[cpu][signature].conf == -1) ||
                    (CSPT_l1[cpu][signature].stride == 0)){
                    // if new entry in CSPT or stride is zero, break
                    break;
                }

                // we are not prefetching at L2 for CPLX type, so encode stride as 0
                trackers_l1[cpu][index].pref_type = CPLX_TYPE;
                metadata = encode_metadata(0, CPLX_TYPE, spec_nl[cpu]);
                if(CSPT_l1[cpu][signature].conf > 0 && i >= CPLX_DIST){                                 // prefetch only when conf>0 for CPLX
                    bl_index = hash_bloom(pf_address);
                    stats[cpu][CPLX_TYPE].bl_request[bl_index] = 1;
                    trackers_l1[cpu][index].pref_type = 3;
#ifdef DO_PREF
                    int found_in_filter = 0;
                    for(int i = 0; i < recent_request_filter.size(); i++)
                    {
                        if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                        {
                            // Prefetch address is present in RR filter
                            found_in_filter = 1;
                        }
                    }
                    //Issue prefetch request only if prefetch address is not present in RR filter
                    if(found_in_filter == 0)
                    {
                        prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                        //Add to RR filter
                        recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                        if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                            recent_request_filter.erase(recent_request_filter.begin());
                    }
#endif
                    num_prefs++;
                    SIG_DP(cout << pref_offset << ", ");
                }
                signature = update_sig_l1(signature, CSPT_l1[cpu][signature].stride);
            }
        }



        // if no prefetches are issued till now, speculatively issue a next_line prefetch
        if(num_prefs == 0 && spec_nl[cpu] == 1){
            if(flag_nl[cpu] == 0)//所以第一次进入next_line类的时候不会预取
                flag_nl[cpu] = 1;
            else {
                uint64_t pf_address = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
                if((pf_address >> LOG2_PAGE_SIZE) != (addr >> LOG2_PAGE_SIZE))
                {
                    // update the IP table entries and return if NL request is not to the same 4 KB page
                    trackers_l1[cpu][index].last_line_offset = line_offset;
                    trackers_l1[cpu][index].last_vpage = curr_page;
                    return;
                }
                bl_index = hash_bloom(pf_address);
                stats[cpu][NL_TYPE].bl_request[bl_index] = 1;
                metadata = encode_metadata(1, NL_TYPE, spec_nl[cpu]);
#ifdef DO_PREF
                int found_in_filter = 0;
                for(int i = 0; i < recent_request_filter.size(); i++)
                {
                    if(recent_request_filter[i] == ((pf_address >> 6) & RR_TAG_MASK))
                    {
                        // Prefetch address is present in RR filter
                        found_in_filter = 1;
                    }
                }
                //Issue prefetch request only if prefetch address is not present in RR filter
                if(found_in_filter == 0)
                {
                    prefetch_line(ip, addr, pf_address, FILL_L1, metadata);
                    //Add to RR filter
                    recent_request_filter.push_back((pf_address >> 6) & RR_TAG_MASK);
                    if(recent_request_filter.size() > NUM_OF_RR_ENTRIES)
                        recent_request_filter.erase(recent_request_filter.begin());
                }
#endif
                trackers_l1[cpu][index].pref_type = NL_TYPE;
                meta_counter[cpu][3]++;
                total_count[cpu]++;
                SIG_DP(cout << "1, ");

                if(acc[cpu][4] < 40)
                    flag_nl[cpu] = 0;
            }                                       // NL IP
        }


        SIG_DP(cout << endl);

        // update the IP table entries
        trackers_l1[cpu][index].last_line_offset = line_offset;
        trackers_l1[cpu][index].last_vpage = curr_page;

    }



    return;
}

void CACHE::l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint64_t metadata_in)
{
    if(prefetch){
        uint32_t pref_type = metadata_in & 0xF00;
        pref_type = pref_type >> 8;

        uint64_t index = hash_bloom(addr);
        if(stats[cpu][pref_type].bl_request[index] == 1){
            stats[cpu][pref_type].bl_filled[index] = 1;
            stats[cpu][pref_type].bl_request[index] = 0;
        }
    }
}
void CACHE::l1d_prefetcher_final_stats()
{
    cout << endl;

    uint64_t total_request=0, total_polluted=0, total_useful=0, total_late = 0;

    for(int i=0; i<5; i++){
        total_request += pref_filled[cpu][i];
        total_polluted += stats[cpu][i].polluted_misses;
        total_useful += pref_useful[cpu][i];
        total_late += pref_late[cpu][i];
    }

    cout << "stream: " << endl;
    cout << "stream:times selected: " << meta_counter[cpu][0] << endl;
    cout << "stream:pref_filled: " << pref_filled[cpu][1] << endl;
    cout << "stream:pref_useful: " << pref_useful[cpu][1] << endl;
    cout << "stream:pref_late: " << pref_late[cpu][1] << endl;
    cout << "stream:misses: " << stats[cpu][1].misses << endl;
    cout << "stream:misses_by_poll: " << stats[cpu][1].polluted_misses << endl;
    cout << endl;

    cout << "CS: " << endl;
    cout << "CS:times selected: " << meta_counter[cpu][1] << endl;
    cout << "CS:pref_filled: " << pref_filled[cpu][2] << endl;
    cout << "CS:pref_useful: " << pref_useful[cpu][2] << endl;
    cout << "CS:pref_late: " << pref_late[cpu][2] << endl;
    cout << "CS:misses: " << stats[cpu][2].misses << endl;
    cout << "CS:misses_by_poll: " << stats[cpu][2].polluted_misses << endl;
    cout << endl;

    cout << "CPLX: " << endl;
    cout << "CPLX:times selected: " << meta_counter[cpu][2] << endl;
    cout << "CPLX:pref_filled: " << pref_filled[cpu][3] << endl;
    cout << "CPLX:pref_useful: " << pref_useful[cpu][3] << endl;
    cout << "CPLX:pref_late: " << pref_late[cpu][3] << endl;
    cout << "CPLX:misses: " << stats[cpu][3].misses << endl;
    cout << "CPLX:misses_by_poll: " << stats[cpu][3].polluted_misses << endl;
    cout << endl;

    cout << "NL_L1: " << endl;
    cout << "NL:times selected: " << meta_counter[cpu][3] << endl;
    cout << "NL:pref_filled: " << pref_filled[cpu][4] << endl;
    cout << "NL:pref_useful: " << pref_useful[cpu][4] << endl;
    cout << "NL:pref_late: " << pref_late[cpu][4] << endl;
    cout << "NL:misses: " << stats[cpu][4].misses << endl;
    cout << "NL:misses_by_poll: " << stats[cpu][4].polluted_misses << endl;
    cout << endl;


    cout << "total selections: " << total_count[cpu] << endl;
    cout << "total_filled: " << pf_fill << endl;
    cout << "total_useful: " << pf_useful << endl;
    cout << "total_late: " << total_late << endl;
    cout << "total_polluted: " << total_polluted << endl;
    cout << "total_misses_after_warmup: " << num_misses[cpu] << endl;

    cout<<"conflicts: " << num_conflicts <<endl;
    cout << endl;

    cout << "test: " << test << endl;
}
