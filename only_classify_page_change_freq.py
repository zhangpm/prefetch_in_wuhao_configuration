import os
import subprocess

def get_files(path):
    traces = os.listdir(path)
    return [path + "/" + trace for trace in traces]

def get_experiment_result(path):
    traces_results_path = get_files(path)
    results = {}
    for path in traces_results_path:
        if "ip_feature_find" in path:
            continue
        trace_name = path.split("/")[-1].split('-')[0]
        f = open(path, "r+", encoding="utf-8")
        for line in f:
            if "CPU 0 cumulative IPC:" in line:
                ipc = line.split(" ")[4]
                results[trace_name] = ipc
                break
        f.close()
        #os.remove(path)
    return results

def read_ip_value(path):

    freq_ips = []
    f = open(path, "r+", encoding="utf-8")
    is_has_time = False

    for line in f:
        content = line.strip().split("|")
        ip = content[0].split(":")[-1]
        per = round(float(content[1].split(":")[-1]), 3)
        freq = int(content[2].split(":")[-1])
        print(ip, per, freq)
        if per < 0.5:
            freq_ips.append(ip)
        else:
            is_has_time = True

    if is_has_time:
        global traces_has_time
        traces_has_time += 1

    f.close()

    return freq_ips

def deal_with_ip(ip_value):
    valuable_ips = []
    for ip in ip_value:
        if ip_value[ip][0] > 0:
            valuable_ips.append(ip)
        # if ip_value[ip][1] >= 1000:
        #     valuable_ips.append(ip)
        else:
            print("valuless ip : {}".format(ip_value[ip]))
    print("important ip num : {}, percentage : {}".format(len(valuable_ips), len(valuable_ips)/len(ip_value)))
    return valuable_ips

def reload_valuable_ips(valuable_ips, reload_valuable_path):
    f = open(reload_valuable_path, "w+", encoding="utf-8")
    for ip in valuable_ips:
        f.write(ip + "\n")
    f.close()

def find_important_ip(trace, prefetcher, n_warm, n_sim, relod_valuable_path):
    cmd = "./run_champsim.sh {} {} {} {}".format(prefetcher, n_warm, n_sim, trace)
    p = subprocess.Popen(cmd, shell=True)
    return_code = p.wait()
    
    freq_ips = read_ip_value("ip_change_page_frequency.txt")
    reload_valuable_ips(freq_ips, relod_valuable_path)
    return True

def compile_prefetcher(branch_predicor, l1d_prefetcher, l2c_prefetcher, llc_prefetcher, llc_replacement, core_num, assoc):
    cmd = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_predicor, l1d_prefetcher, l2c_prefetcher, llc_prefetcher, llc_replacement, core_num, assoc)
    p = subprocess.Popen(cmd, shell=True)
    return_code = p.wait()

if __name__ == '__main__':
    traces_has_time = 0
    trace = ""
    trace_dir = "/home/hkucs/ljz/dpc3_traces"
    result_dir = "results_10M"
    valuable_fir = "important_ips.txt"
    valuelss_fir = "valuless_ips.txt"
    n_warm = 200
    n_sim = 50
    ip_valuable_analysisor = "perceptron-ip_page_change_frequency-no-no-lru-1core-assoc2"
    ip_classify_paper = "perceptron-ipcp_mix_time_absolute_classify-ipcp_mix_time_absolute_classify-no-lru-1core-assoc2"
    #ip_classify_paper_compare = "bimodal-no-paper_ipcp_ip_classify_v1-paper_ipcp-no-lru-1core"

    #build
    branch_predicor = "perceptron"
    l1d_prefetcher = "ip_page_change_frequency"
    l2c_prefetcher = "no"
    llc_prefetcher = "no"
    llc_replacement = "lru"
    core_num = "1"
    assoc = "2"

    #compile
    print("Start compile {} {} {} {} {} {} {}...".format(branch_predicor, l1d_prefetcher, l2c_prefetcher, llc_prefetcher, llc_replacement, core_num, assoc))
    compile_prefetcher(branch_predicor, l1d_prefetcher, l2c_prefetcher, llc_prefetcher, llc_replacement, core_num, assoc)

    l1d_prefetcher = "ipcp_mix_time_absolute_classify"
    l2c_prefetcher = "ipcp_mix_time_absolute_classify"
    compile_prefetcher(branch_predicor, l1d_prefetcher, l2c_prefetcher, llc_prefetcher, llc_replacement, core_num, assoc)

    #make experiment
    traces = os.listdir(trace_dir)
    for trace in traces:
        print("Start make experiment on {}".format(trace))
        print("Start find valuable ips ...")
        #bimodal-no-ipcp_ip_value-ipcp-ipcp-lru-1core
        find_important_ip(trace, ip_valuable_analysisor, n_warm, n_sim, valuable_fir)
        #print("Start make experiment ...")

        cmd = "./run_champsim.sh {} {} {} {}".format(ip_classify_paper, n_warm, n_sim, trace)
        p = subprocess.Popen(cmd, shell=True)
        return_code = p.wait()

    print("Num of trace has time ip :", traces_has_time)
