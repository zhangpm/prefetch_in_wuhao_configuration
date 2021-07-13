pairs_num = 15
cur_ip = ""

def adds_to_str(addr1, addr2):
    return str(addr1) + "-" + str(addr2)

def str_to_adds(addstr):
    return int(addstr.split("-")[0]), int(addstr.split("-")[1])
    
def is_first_addr_exist(cache_line, addr_pairs):#判断这个first_cache_line是否存在
    for i in range(0,len(addr_pairs)):
        if cache_line == addr_pairs[i]["first_cache"]:
            return i
    return -1

def update_info(addr_pairs, index, old_second_cache, new_second_cache):
    for addr_pair in addr_pairs:
        addr_pair["lru"] += 1
    addr_pairs[index]["lru"] = 0
    if(old_second_cache == new_second_cache):
        addr_pairs[index]["conf"] += 1
        if(addr_pairs[index]["conf"] > 3):
            addr_pairs[index]["conf"] = 3
        return True
    else:
        addr_pairs[index]["conf"] -= 1
        if(addr_pairs[index]["conf"] < 0):
            addr_pairs[index]["conf"] = 0
        addr_pairs[index]["second_cache"] = new_second_cache
        return False

def add_new_pair(addr_pairs, first_cache, second_cache):#由addr_pairs里面的lru成员来筛选需要被替换的元素
    for addr_pair in addr_pairs:
        addr_pair["lru"] += 1
    if(len(addr_pairs) < pairs_num):#直接加入一个新的元素
        dict_temp = dict()
        dict_temp["first_cache"] = first_cache
        dict_temp["second_cache"] = second_cache
        dict_temp["conf"] = 0
        dict_temp["lru"] = 0
        addr_pairs.append(dict_temp)
    else:
        lru = addr_pairs[0]["lru"]
        conf = addr_pairs[0]["conf"]
        index = 0
        for i in range(1, len(addr_pairs)):#找出lru最小的元素更换
            if(addr_pairs[i]["lru"] > lru or (addr_pairs[i]["lru"] == lru and addr_pairs[i]["conf"] < conf)):
                index = i
        addr_pairs[index]["first_cache"] = first_cache
        addr_pairs[index]["second_cache"] = second_cache
        addr_pairs[index]["conf"] = 0
        addr_pairs[index]["lru"] = 0


def count_pattern_coverage(cache_line_list, cover_list, write_file):#读一个ip的pattern，并计算覆盖率
    predict_num = 0
    addr_pairs = []
    for i in range(0, len(cache_line_list)-1):
        index = is_first_addr_exist(cache_line_list[i], addr_pairs)
        if(-1 != index):#如果这个地址已经作为开始地址记录了
            cover_list[i] = 1
            if update_info(addr_pairs, index, addr_pairs[index]["second_cache"], cache_line_list[i+1]):
                cover_list[i+1] = 1
                predict_num += 1
        else:
            add_new_pair(addr_pairs, cache_line_list[i], cache_line_list[i+1])
    cover_num = 0
    for is_cover in cover_list:
        cover_num += is_cover
    if(cover_num > 0):
        write_file.write(cur_ip)
        write_file.write("单个pattern覆盖率：" + str(cover_num/len(cover_list)) + "\t覆盖数量：" + str(cover_num) + "\n")
        write_file.write("单个pattern预测成功覆盖率：" + str(predict_num/len(cover_list)) + "\t覆盖数量" + str(predict_num) + "\n")
    return cover_num, predict_num

def count_pattern_coverage_old(cache_line_list, cover_list, write_file , num):#读一个ip的pattern，并用旧的策略计算覆盖率
    adds_dict = dict()
    for i in range(0, len(cache_line_list)-1):
        temp_str = adds_to_str(cache_line_list[i], cache_line_list[i+1])
        if(temp_str in adds_dict):
            adds_dict[temp_str] += 1
        else:
            adds_dict[temp_str] = 1
    #按照出现的次数倒序排列
    sorted_pairs = sorted(adds_dict.items(), key=lambda kv: (kv[1], kv[0]), reverse=True)
    exist_addrs = []
    count_num = 0
    for sorted_pair in sorted_pairs:
        if(count_num >= num):
            break
        if sorted_pair[1] < 2:
            break
        addr1, addr2 = str_to_adds(sorted_pair[0])
        if addr1 in exist_addrs:
            continue
        for i in range(0, len(cache_line_list)-1):
            if(addr1 == cache_line_list[i] and addr2 == cache_line_list[i+1]):
                cover_list[i] = 1
                cover_list[i+1] = 1
        count_num += 1
        exist_addrs.append(addr1)
    cover_num = 0
    for is_cover in cover_list:
        cover_num += is_cover
    if (cover_num > 0):
        write_file.write(cur_ip)
        write_file.write("单个pattern覆盖率：" + str(cover_num / len(cover_list)) + "\t覆盖数量：" + str(cover_num) + "\n")
    return cover_num, cover_num

def coverage_count(file_path):
    write_file = open(file_path + str(pairs_num) + "-" + "coverage_result.txt", "w+", encoding="utf-8")
    pattern_list = []  # 用于存一个ip对映体的pattern（内容是对应的cache块号）
    cover_list = []
    important_pattern_num = 0
    total_pattern_num = 1
    total_cover_num = 0
    predict_total_num = 0
    with open("target_ip_pattern.txt", "r+", encoding="utf-8") as ips_pattern:
        for line in ips_pattern:
            if "total_pattern_num" in line:
                total_pattern_num = int(line.split(":")[1])
            elif "ip" in line:  # 开始一个pattern
                pattern_list.clear()
                cover_list.clear()
                global cur_ip
                cur_ip = line
            elif "addr" in line:
                addr = int(line.split(" ")[2])
                cache_line = addr >> 6
                pattern_list.append(cache_line)
                cover_list.append(0)
            else:
                cover_num, predict_num = count_pattern_coverage(pattern_list, cover_list, write_file)
                total_cover_num += cover_num
                predict_total_num += predict_num
                important_pattern_num += len(cover_list)
    write_file.write("时间特性pattern下的总覆盖率：" + str(total_cover_num / important_pattern_num) + "\t覆盖数量：" + str(total_cover_num) + "\n")
    write_file.write("时间特征pattern下预测成功覆盖率：" + str(predict_total_num / important_pattern_num) + "\t覆盖数量" + str(predict_total_num) + "\n")
    write_file.write("时间特性pattern下的总覆盖率：" + str(total_cover_num / total_pattern_num) + "\t覆盖数量：" + str(total_cover_num) + "\n")
    write_file.write("时间特征pattern下预测成功覆盖率：" + str(predict_total_num / total_pattern_num) + "\t覆盖数量" + str(predict_total_num) + "\n")
    write_file.close()

def coverage_count_old(file_path):
    write_file = open(file_path + str(pairs_num) + "-" + "coverage_result_old.txt", "w+", encoding="utf-8")
    pattern_list = []  # 用于存一个ip对映体的pattern（内容是对应的cache块号）
    cover_list = []
    important_pattern_num = 0
    total_pattern_num = 1
    total_cover_num = 0
    predict_total_num = 0
    with open("target_ip_pattern.txt", "r+", encoding="utf-8") as ips_pattern:
        for line in ips_pattern:
            if "total_pattern_num" in line:
                total_pattern_num = int(line.split(":")[1])
            elif "ip" in line:  # 开始一个pattern
                pattern_list.clear()
                cover_list.clear()
                global cur_ip
                cur_ip = line
            elif "addr" in line:
                addr = int(line.split(" ")[2])
                cache_line = addr >> 6
                pattern_list.append(cache_line)
                cover_list.append(0)
            else:
                cover_num, predict_num = count_pattern_coverage_old(pattern_list, cover_list, write_file, 10)
                total_cover_num += cover_num
                predict_total_num += predict_num
                important_pattern_num += len(cover_list)
    write_file.write(
        "时间特性pattern下的总覆盖率：" + str(total_cover_num / important_pattern_num) + "\t覆盖数量：" + str(total_cover_num) + "\n")
    write_file.write("时间特征pattern下预测成功覆盖率：" + str(predict_total_num / important_pattern_num) + "\t覆盖数量" + str(
        predict_total_num) + "\n")
    write_file.write(
        "时间特性pattern下的总覆盖率：" + str(total_cover_num / total_pattern_num) + "\t覆盖数量：" + str(total_cover_num) + "\n")
    write_file.write(
        "时间特征pattern下预测成功覆盖率：" + str(predict_total_num / total_pattern_num) + "\t覆盖数量" + str(predict_total_num) + "\n")

if __name__ == '__main__':
    coverage_count("coverage_result.txt")


