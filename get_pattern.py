import os
import subprocess

trace_dir = "/home/hkucs/ljz/dpc3_traces1"
branch_name = "perceptron"
core_num = 1
assoc_num = 2
repl_name = "lru"
n_warm = 10
n_sim = 50

def os_process(process_str):
    p = subprocess.Popen(process_str, shell=True)
    return_code = p.wait()
    return return_code

def run_binary(binary_name, result_dir):
    #编译
    process_str = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_name, binary_name.split("-")[1], binary_name.split("-")[2], binary_name.split("-")[3], repl_name, core_num, assoc_num)
    os_process(process_str)
    for trace_name in os.listdir(trace_dir):
        #执行
        process_str = "./run_champsim.sh {} {} {} {}".format(binary_name, n_warm, n_sim, trace_name)
        os_process(process_str)
        if not os.path.isdir(result_dir):
            process_str = "mkdir {}".format(result_dir)
            os_process(process_str)
        process_str = "mv target_ip_pattern.txt {}/{}ip_pattern.txt".format(result_dir, trace_name)
        os_process(process_str)
        process_str = "mv target_mix_pattern.txt {}/{}mix_pattern.txt".format(result_dir, trace_name)
        os_process(process_str)

if __name__=="__main__":
    binary_name = "perceptron-find_pattern-no-no-lru-1core-assoc2"
    run_binary(binary_name, "pattern_result")