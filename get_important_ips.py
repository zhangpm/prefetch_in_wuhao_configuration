import subprocess

def read_ip_value(path):

    freq_ips = []
    f = open(path, "r+", encoding="utf-8")
    is_has_time = False

    for line in f:
        content = line.strip().split("|")
        ip = content[0].split(":")[-1]
        per = round(float(content[1].split(":")[-1]), 3)
        freq = int(content[2].split(":")[-1])
        print(ip, per, freq)
        if per < 0.5:
            freq_ips.append(ip)
        else:
            is_has_time = True

    if is_has_time:
        global traces_has_time
        traces_has_time += 1

    f.close()

    return freq_ips

def reload_valuable_ips(valuable_ips, reload_valuable_path):
    f = open(reload_valuable_path, "w+", encoding="utf-8")
    for ip in valuable_ips:
        f.write(ip + "\n")
    f.close()


def find_important_ip(relod_valuable_path):
    freq_ips = read_ip_value("ip_change_page_frequency.txt")
    reload_valuable_ips(freq_ips, relod_valuable_path)

if __name__ == '__main__':
    traces_has_time = 0
    find_important_ip("important_ips.txt")
