import subprocess
import time
import random
import re
import os

open("out.txt", "w", encoding="utf-8").close()
_original_print = print

def print(*args, **kwargs):
    _original_print(*args, **kwargs)
    with open("out.txt", "a", encoding="utf-8") as f:
        _original_print(*args, file=f, **kwargs)

# 压测核心配置
random.seed(42)

DB_CMD = ["./bin/observer", "-f", "../etc/observer.ini", "-P", "cli"]
DIM = 128
TOP_K = 10
DATA_SCALES = [1000, 10000, 100000]
QUERY_COUNT = 20  # 每次取20个不同向量进行批量查询，求平均召回率

TEST_PARAMS = [
    {"lists": 10, "probes": 1},
    {"lists": 50, "probes": 2},
    {"lists": 100, "probes": 5},
    {"lists": 500, "probes": 10},
    {"lists": 1000, "probes": 20}
]

# 在 128 维空间中生成 100 个真实的聚类中心
NUM_CLUSTERS = 100
base_centers = [[random.uniform(-1.0, 1.0) for _ in range(DIM)] for _ in range(NUM_CLUSTERS)]

def generate_clustered_vector():
    """生成带有高斯扰动的聚集特征向量"""
    center = random.choice(base_centers)
    # 在聚类中心周边产生 0.3 的随机扰动
    return "[" + ",".join([str(round(c + random.uniform(-0.3, 0.3), 2)) for c in center]) + "]"

def run_script(sql_script):
    start = time.time()
    p = subprocess.Popen(DB_CMD, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out, _ = p.communicate(input=sql_script + "\nEXIT;\n")
    end = time.time()
    return out, end - start

def extract_id_chunks(cli_output):
    all_ids = []
    for line in cli_output.split('\n'):
        match = re.match(r'^(\d+)\s*\|', line.strip())
        if match:
            all_ids.append(int(match.group(1)))
    chunks = []
    for i in range(QUERY_COUNT):
        chunks.append(all_ids[i*TOP_K : (i+1)*TOP_K])
    return chunks

for scale in DATA_SCALES:
    print(f"\n测试数据规模: {scale} Rows | 批量查询数: {QUERY_COUNT} 次")
    table_name = f"perf_{scale}_{int(time.time())}"
    
    print("生成 SQL 脚本并写入数据")
    setup_sql = f"CREATE TABLE {table_name}(id int, embedding vector({DIM}));\nBEGIN;\n"
    inserts = []
    for i in range(1, scale + 1):
        inserts.append(f"INSERT INTO {table_name} VALUES({i}, STRING_TO_VECTOR('{generate_clustered_vector()}'));")
    setup_sql += "\n".join(inserts) + "\nCOMMIT;\n"
    
    _, load_time = run_script(setup_sql)
    print(f"写入完成，耗时: {load_time:.2f} s")
    
    query_vecs = [generate_clustered_vector() for _ in range(QUERY_COUNT)]
    queries_sql = "".join([f"SELECT id, DISTANCE(embedding, STRING_TO_VECTOR('{v}'), 'L2_DISTANCE') AS dis FROM {table_name} ORDER BY dis LIMIT {TOP_K};\n" for v in query_vecs])
    
    print("正在执行精确检索 (Baseline)...")
    _, t_base = run_script("") 
    exact_out, t_exact_raw = run_script(queries_sql) 
    
    exact_time_avg = max(0.01, (t_exact_raw - t_base) * 1000 / QUERY_COUNT)
    ground_truth_chunks = extract_id_chunks(exact_out)
    print(f"[Baseline] 平均耗时: {exact_time_avg:8.2f} ms")
    
    for param in TEST_PARAMS:
        l, p = param["lists"], param["probes"]
        if l > scale // 2: continue
            
        idx_sql = f"CREATE VECTOR INDEX idx_{l}_{p} ON {table_name}(embedding) WITH (lists={l}, probes={p});\n"
        
        _, t_idx_base = run_script(idx_sql)
        approx_out, t_idx_and_selects = run_script(idx_sql + queries_sql)
        
        approx_time_avg = max(0.01, (t_idx_and_selects - t_idx_base) * 1000 / QUERY_COUNT)
        approx_chunks = extract_id_chunks(approx_out)
        
        total_recall = 0.0
        for gt_ids, app_ids in zip(ground_truth_chunks, approx_chunks):
            intersection = set(gt_ids).intersection(set(app_ids))
            total_recall += (len(intersection) / TOP_K) if TOP_K > 0 else 0
        avg_recall = total_recall / QUERY_COUNT if QUERY_COUNT > 0 else 0
        
        print(f"[IVF_Flat L={l:4d}, P={p:2d}]  平均耗时: {approx_time_avg:8.2f} ms  平均召回率: {avg_recall*100:5.1f}%")

print("\n压测执行完毕，数据已生成于 out.txt。")