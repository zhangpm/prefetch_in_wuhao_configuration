import os
import subprocess

brach = "perceptron"
l1d = "time_mixer"
l2c = "time_mixer"
llc = "no"
repl = "lru"
core_num = "1"
assoc = "2"

n_warm = 200
n_sim = 50

traces = os.listdir("/home/hkucs/ljz/dpc3_traces")

cmd = "./build_champsim.sh {} {} {} {} {} {} {}".format(brach, l1d, l2c, llc, repl, core_num, assoc)
p = subprocess.Popen(cmd, shell=True)
return_code = p.wait()

for trace in traces:
    print("start trace" + trace)
    os.system("./run_champsim.sh perceptron-time_mixer-time_mixer-no-lru-1core-assoc2 {} {} {}".format(n_warm, n_sim, trace))
    print("trace finish" + trace)
