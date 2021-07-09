import os
import subprocess

trace_dir = "/home/hkucs/ljz/dpc3_traces"
branch_name = "perceptron"
core_num = 1
assoc_num = 2
repl_name = "lru"
binary_name0 = "perceptron-time_mixer-time_mixer-no-lru-1core-assoc2"
n_warm = 10
n_sim = 50

def os_process(process_str):
    p = subprocess.Popen(process_str, shell=True)
    return_code = p.wait()
    return return_code

if __name__=="__main__":
    for trace_name in os.listdir(trace_dir):
        if("483.xalancbmk-736B" not in trace_name):
            continue
        #编译
        process_str = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_name, binary_name0.split("-")[1], binary_name0.split("-")[2], binary_name0.split("-")[3], repl_name, core_num, assoc_num)
        os_process(process_str)
        #执行
        process_str = "./run_champsim.sh {} {} {} {}".format(binary_name0, n_warm, n_sim, trace_name)
        os_process(process_str)