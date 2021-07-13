import os
import subprocess
from coverage_count import coverage_count, coverage_count_old

trace_dir = "/home/hkucs/ljz/dpc3_traces"
branch_name = "perceptron"
core_num = 1
assoc_num = 2
repl_name = "lru"
binary_name0 = "perceptron-ip_page_change_frequency-no-no-lru-1core-assoc2"
n_warm = 10
n_sim = 50
binary_name1 = "perceptron-find_pattern-no-no-lru-1core-assoc2"

def os_process(process_str):
    p = subprocess.Popen(process_str, shell=True)
    return_code = p.wait()
    return return_code

def read_ip_value(path):

    freq_ips = []
    f = open(path, "r+", encoding="utf-8")

    for line in f:
        content = line.strip().split("|")
        ip = content[0].split(":")[-1]
        per = round(float(content[1].split(":")[-1]), 3)
        freq = int(content[2].split(":")[-1])
        print(ip, per, freq)
        if per < 0.5:
            freq_ips.append(ip)

    f.close()

    return freq_ips

def reload_valuable_ips(valuable_ips, reload_valuable_path):
    f = open(reload_valuable_path, "w+", encoding="utf-8")
    for ip in valuable_ips:
        f.write(ip + "\n")
    f.close()


def find_important_ip(relod_valuable_path):
    freq_ips = read_ip_value("ip_change_page_frequency.txt")
    reload_valuable_ips(freq_ips, relod_valuable_path)

if __name__ == '__main__':
    trace_names = os.listdir(trace_dir)
    for trace_name in trace_names:
        #首先执行以获取重要的ip
        process_str = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_name, binary_name0.split("-")[1], binary_name0.split("-")[2], binary_name0.split("-")[3], repl_name, core_num, assoc_num)
        os_process(process_str)
        process_str = "./run_champsim.sh {} {} {} {}".format(binary_name0, n_warm, n_sim, trace_name)
        os_process(process_str)
        #找出没有时间特征的ip写人文件
        find_important_ip("important_ips.txt")
        #将具有时间特征的ip的pattern写入文件
        process_str = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_name, binary_name1.split("-")[1], binary_name1.split("-")[2], binary_name1.split("-")[3], repl_name, core_num, assoc_num)
        os_process(process_str)
        process_str = "./run_champsim.sh {} {} {} {}".format(binary_name1, n_warm, n_sim, trace_name)
        os_process(process_str)
        #统计覆盖率
        coverage_count_old("coverage_result/" + trace_name + "—" + str(n_warm) + "-" + str(n_sim) + "-")
        process_str = "mv target_ip_pattern.txt coverage_result/{}pattern.txt".format(trace_name + "—" + str(n_warm) + "-" + str(n_sim) + "-")
        os_process(process_str)
