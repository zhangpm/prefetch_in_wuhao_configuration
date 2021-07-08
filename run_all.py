import os
binary_dir = "bin"
trace_dir = "/home/hkucs/ljz/dpc3_traces"
n_warm = 200
n_sim = 50

binarys = os.listdir(binary_dir)
for binary in binarys:
    if("ipcp_isca2020" not in binary):
        continue
    print(binary + " start")
    traces = os.listdir(trace_dir)
    for trace in traces:
        print(trace + " start")
        os.system("./run_champsim.sh {} {} {} {}".format(binary, n_warm, n_sim, trace))
        print(trace + " finish")
