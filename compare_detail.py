import os

compare_dirs = ["bo_percore_result", "bo_triage_result", "triage_result", "ipcp_result", "time_mixer_result"]
#compare_traces = ["mcf", "omnetpp", "xalancbmk"]
rjust_len = 20

def get_ipc(path):
    f = open(path, "r+", encoding = "utf-8")
    for line in f:
        if("CPU 0 cumulative IPC" in line):
            f.close()
            return float(line.split(" ")[4].strip())

def get_ipc_by_trace(dir, trace):
    file_names = os.listdir(dir)
    for file_name in file_names:
        if trace in file_name:
            return get_ipc(dir + "/" + file_name)

def get_ipc_average(dir, trace):
    total_ipc = 0
    total_num = 0
    for file_name in os.listdir(dir):
        if trace in file_name:
            total_num += 1
            total_ipc += get_ipc(dir + "/" +file_name)
    return round(total_ipc/total_num, 6)

if __name__ == "__main__":
    write_file = open("compare.txt", "w+", encoding = "utf-8")
    write_file.write("trace_name".rjust(rjust_len))
    for compare_dir in compare_dirs:
        write_file.write(compare_dir.rjust(rjust_len))
    write_file.write("\n")

    compare_traces = sorted(os.listdir(compare_dirs[0]))
    for i in range(0, len(compare_traces)):
        compare_traces[i] = compare_traces[i].split("-perceptron")[0]
    for compare_trace in compare_traces:
        write_file.write(compare_trace.rjust(rjust_len))
        for compare_dir in compare_dirs:
            write_file.write(str(get_ipc_by_trace(compare_dir, compare_trace)).rjust(rjust_len))
        write_file.write("\n")
    write_file.close()