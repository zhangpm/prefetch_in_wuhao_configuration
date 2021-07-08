import os
trace_dir = "/home/hkucs/ljz/dpc3_traces"
n_warm = 200
n_sim = 50

traces = os.listdir(trace_dir)
for trace in traces:
    print(trace + " start")
    os.system("./run_champsim.sh bimodal-no-triage-no-lru-1core-assoc2 {} {} {}".format(n_warm, n_sim, trace))
    print(trace + "finish")
