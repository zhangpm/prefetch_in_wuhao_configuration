import os

baseline_dir = "baseline_result/"
bo_dir = "bo_percore_result/"
bo_triage_dir = "bo_triage_result/"
triage_dir = "triage_result/"
ipcp_dir = "ipcp_isca2020_result/"
offline_dir = "offline_result/"
online_dir = "online_result/"

child_dirs = ["mcf", "omnetpp", "xalancbmk"]
result_rjust_len = 20

def get_ipc(path):
    f = open(path, "r+", encoding = "utf-8")
    for line in f:
        if("CPU 0 cumulative IPC" in line):
            f.close()
            return line.split(" ")[4].strip()

write_file = open("compare.txt", "w+", encoding = "utf-8")
write_file.write("trace_name".rjust(40, " ") + baseline_dir.rjust(result_rjust_len, " ") + bo_dir.rjust(result_rjust_len, " ") + bo_triage_dir.rjust(result_rjust_len, " ") + triage_dir.rjust(result_rjust_len, " ") + "ipcp".rjust(result_rjust_len) + "offline".rjust(result_rjust_len) + "online".rjust(result_rjust_len))
write_file.write("\n")

for child_dir in child_dirs:
    baseline_results = os.listdir(baseline_dir+child_dir)
    baseline_results.sort()
    bo_results = os.listdir(bo_dir+child_dir)
    bo_triage_results = os.listdir(bo_triage_dir+child_dir)
    triage_results = os.listdir(triage_dir+child_dir)
    ipcp_results = os.listdir(ipcp_dir+child_dir)
    offline_results = os.listdir(offline_dir+child_dir)
    online_results = os.listdir(online_dir+child_dir)


    for baseline_result in baseline_results:
        trace_name = baseline_result.split("perceptron")[0]
        write_file.write(trace_name.rjust(40, " "))
        baseline_ipc = float(get_ipc(baseline_dir + child_dir + "/" + baseline_result))
        write_file.write(str(baseline_ipc).rjust(result_rjust_len, " "))

        flag = True
        for bo_result in bo_results:
            if trace_name in bo_result:
                bo_ipc = float(get_ipc(bo_dir + child_dir + "/" + bo_result))
                speed_up = round(bo_ipc/baseline_ipc-1 , 3)
                write_file.write((str(bo_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        flag = True
        for bo_triage_result in bo_triage_results:
            if trace_name in bo_triage_result:
                bo_triage_ipc = float(get_ipc(bo_triage_dir + child_dir + "/" + bo_triage_result))
                speed_up = round(bo_triage_ipc/baseline_ipc-1, 3)
                write_file.write((str(bo_triage_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        flag = True
        for triage_result in triage_results:
            if trace_name in triage_result:
                triage_ipc = float(get_ipc(triage_dir + child_dir + "/" + triage_result))
                speed_up = round(triage_ipc/baseline_ipc-1, 3)
                write_file.write((str(triage_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        flag = True
        for ipcp_result in ipcp_results:
            if trace_name in ipcp_result:
                ipcp_ipc = float(get_ipc(ipcp_dir + child_dir + "/" + ipcp_result))
                speed_up = round(ipcp_ipc / baseline_ipc - 1, 3)
                write_file.write((str(ipcp_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        flag = True
        for offline_result in offline_results:
            if trace_name in offline_result:
                ipcp_ipc = float(get_ipc(offline_dir + child_dir + "/" + offline_result))
                speed_up = round(ipcp_ipc / baseline_ipc - 1, 3)
                write_file.write((str(ipcp_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        flag = True
        for online_result in online_results:
            if trace_name in online_result:
                ipcp_ipc = float(get_ipc(online_dir + child_dir + "/" + online_result))
                speed_up = round(ipcp_ipc / baseline_ipc - 1, 3)
                write_file.write((str(ipcp_ipc) + "(" + str(speed_up) + ")").rjust(result_rjust_len, " "))
                flag = False
                break
        if flag:
            write_file.write("none".rjust(result_rjust_len, " "))

        write_file.write("\n")
write_file.close()
