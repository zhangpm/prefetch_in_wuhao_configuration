import os

prefetchers = os.listdir("bin")
traces = os.listdir("dpc3_traces")

for trace in traces:
    os.system("./run_champsim.sh bimodal-no-bo_percore-no-lru-1core-assoc2 50 50 {}".format(trace))
