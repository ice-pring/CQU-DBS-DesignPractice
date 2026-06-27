import subprocess
import time
import random
import re
import os
import datetime


open("out.txt", "w", encoding="utf-8").close()
_original_print = print

def print(*args, **kwargs):
    """重载 print 函数，实现终端显示与文件写入同步"""
    _original_print(*args, **kwargs)  # 保持终端输出
    with open("out.txt", "a", encoding="utf-8") as f:
        _original_print(*args, file=f, **kwargs)  # 追加写入文件


random.seed(42)

# 压测核心配置
DB_CMD = ["./bin/observer", "-f", "../etc/observer.ini", "-P", "cli"]
DIM = 4
TOP_K = 10
DATA_SCALES = [1000, 10000, 100000]

TEST_PARAMS = [
    {"lists": 10, "probes": 1},
    {"lists": 50, "probes": 2},
    {"lists": 100, "probes": 5},
    {"lists": 500, "probes": 10},  # 专为 10w 和 100w 规模设计
    {"lists": 1000, "probes": 20}  # 专为 100w 规模设计
]

def run_script(sql_script):
    start = time.time()
    p = subprocess.Popen(DB_CMD, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out, _ = p.communicate(input=sql_script + "\nEXIT;\n")
    end = time.time()
    return out, end - start

def extract_ids(cli_output):
    ids = []
    for line in cli_output.split('\n'):
        match = re.match(r'^(\d+)\s*\|', line.strip())
        if match:
            ids.append(int(match.group(1)))
    return ids

def generate_random_vector():
    return "[" + ",".join([str(round(random.uniform(-10, 10), 4)) for _ in range(DIM)]) + "]"

for scale in DATA_SCALES:
    print(f"\n测试数据规模: {scale} Rows")
    table_name = f"perf_{scale}_{int(time.time())}"
    
    print("生成 SQL 脚本")
    setup_sql = f"CREATE TABLE {table_name}(id int, embedding vector({DIM}));\nBEGIN;\n"
    inserts = []
    for i in range(1, scale + 1):
        inserts.append(f"INSERT INTO {table_name} VALUES({i}, STRING_TO_VECTOR('{generate_random_vector()}'));")
    setup_sql += "\n".join(inserts) + "\nCOMMIT;\n"
    
    _, load_time = run_script(setup_sql)
    print(f"写入数据，耗时: {load_time:.2f} s")
    
    query_vec = generate_random_vector()
    query_sql = f"SELECT id, DISTANCE(embedding, STRING_TO_VECTOR('{query_vec}'), 'L2_DISTANCE') AS dis FROM {table_name} ORDER BY dis LIMIT {TOP_K};\n"
    
    _, t_base = run_script("") 
    exact_out, t_exact_raw = run_script(query_sql * 10) 
    
    exact_time = max(0.01, (t_exact_raw - t_base) * 1000 / 10)
    ground_truth_ids = extract_ids(exact_out)[:TOP_K] 
    print(f"执行精确检索，耗时: {exact_time:8.2f} ms")
    
    for param in TEST_PARAMS:
        l, p = param["lists"], param["probes"]
        
        # 优化：数据量不足时不创建过多的聚类簇
        if l > scale // 2:
            continue
            
        idx_sql = f"CREATE VECTOR INDEX idx_{l}_{p} ON {table_name}(embedding) WITH (lists={l}, probes={p});\n"
        
        _, t_idx_base = run_script(idx_sql)
        approx_out, t_idx_and_selects = run_script(idx_sql + (query_sql * 10))
        
        approx_time = max(0.01, (t_idx_and_selects - t_idx_base) * 1000 / 10)
        approx_ids = extract_ids(approx_out)[:TOP_K]
        
        intersection = set(ground_truth_ids).intersection(set(approx_ids))
        recall = len(intersection) / TOP_K if TOP_K > 0 else 0
        
        print(f"[IVF_Flat L={l:4d}, P={p:2d}]  耗时: {approx_time:8.2f} ms  召回率: {recall*100:5.1f}%")

print("\n压测执行完毕")