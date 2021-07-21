import os

old_dir = "result_of_3_mixstream"
new_dir = "result_of_3_mixstream2l2"

old_traces = os.listdir(old_dir)
new_traces = os.listdir(new_dir)

def get_ipc(path):
    try:
        f = open(path, "r+", encoding = "utf-8")
        for line in f:
            if("CPU 0 cumulative IPC" in line):
                f.close()
                return float(line.split(" ")[4].strip())
    except FileNotFoundError:
        return 10000

def get_ipc_bytrace(trace_name):
    for file_name in os.listdir(new_dir):
        if trace_name in file_name:
            return get_ipc(new_dir + "/" + file_name)


write_file = open("compare2dir.txt", "w+", encoding="utf-8")
write_file.write("trace_name".rjust(40) + "old_ipc".rjust(25) + "new_ipc".rjust(25) + "\n")
for old_trace in old_traces:
    old_ipc = get_ipc(old_dir + "/" + old_trace)
    new_ipc = get_ipc_bytrace(old_trace.split("perceptron")[0])
    write_file.write(old_trace.split("perceptron")[0].rjust(40))
    write_file.write("1".rjust(25))
    write_file.write(str(new_ipc/old_ipc - 1).rjust(25))
    write_file.write("\n")
write_file.close()

