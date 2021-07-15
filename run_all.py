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

def get_important_ips(trace_name):
    coverage_dir = "coverage_result/old_coverage/"
    writefile = open("import_ips.txt", "w+", encoding="utf-8")
    with open(coverage_dir + trace_name + "—10-50-15-coverage_result_old.txt", "r+", encoding="utf-8") as readfile:
        while True:
            first_line = readfile.readline()
            if("ip" in first_line):
                ip_value = first_line.split(":")[1].strip()
                second_line = readfile.readline()
                percent = float(second_line.split("\t")[0].split("：")[1].strip())
                num = int(second_line.split("\t")[1].split("：")[1].strip())
                if percent > 0.1 and num > 1000:
                    writefile.write(ip_value + "\n")
            else:
                break
    writefile.close()

if __name__=="__main__":
    #编译
    process_str = "./build_champsim.sh {} {} {} {} {} {} {}".format(branch_name, binary_name0.split("-")[1], binary_name0.split("-")[2], binary_name0.split("-")[3], repl_name, core_num, assoc_num)
    os_process(process_str)
    for trace_name in os.listdir(trace_dir):
        get_important_ips(trace_name)
        #执行
        process_str = "./run_champsim.sh {} {} {} {}".format(binary_name0, n_warm, n_sim, trace_name)
        os_process(process_str)