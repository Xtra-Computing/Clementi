## This file aims to generate the memory acccess operation numbers for different graph datasets, which consists of 
## vertex memory access operation numbers AND edge memory access operation numbers AND v_e memory access operation number ratio.
## Please note that the MTEPS in ./m_access folder is measured by actual hw execution on FPGAs.

import numpy as np
import multiprocessing
import sys
import os
import psutil

BURST_LENGTH = 32
Granularity = 16

base_path = "/data/yufeng/4node_4SLR_graph_dataset/"  # Replace with the actual file path
base_sub_partition = 16
dataset_name = ["R24", "G24", "R19", "WP", "LJ", "Friendster", "TW", "UK2007-05", "G25"] ## for large real graph processing
# dataset_name = ["R25-2-0", "R25-2-1", "R25-2-2", "R25-2-3",
                # "R25-4-0", "R25-4-1", "R25-4-2", "R25-4-3",
                # "R25-8-0", "R25-8-1", "R25-8-2", "R25-8-3",
                # "R25-16-0", "R25-16-1", "R25-16-2", "R25-16-3",
                # "R25-32-0", "R25-32-1", "R25-32-2", "R25-32-3",
                # "R25-64-0", "R25-64-1", "R25-64-2", "R25-64-3"] ## for large real graph processing

# Function to read a binary file and return its contents as a NumPy array
def read_binary_file(filename):
    with open(filename, 'rb') as file:
        data = np.fromfile(file, dtype=np.int32)
        data = data.reshape(-1, 2)
        data = data[(data[:, 1] != -2)][1:]
    return data

for dataset in dataset_name:

    file_path = base_path + dataset
    partition_num = 0
    for p in range(100):
        ck_file_name = file_path + "/p_" + str(p) + "_sp_0.bin"
        if os.path.exists(ck_file_name):
            partition_num = p + 1
    print ("dataset = ", dataset, " partition_num = ", partition_num)
    
    v_op_num = np.zeros((partition_num), dtype = np.int32)
    e_op_num = np.zeros((partition_num), dtype = np.int32)

    for partition in range(partition_num):
        file_names = []
        for sp_idx in range(base_sub_partition):
            file_names.append(base_path + dataset + "/p_" + str(partition) + "_sp_" + str(sp_idx) + ".bin")
            
        # Create a multiprocessing Pool with base_sub_partition processes
        with multiprocessing.Pool(processes=base_sub_partition) as pool:
            # Use pool.map to read each binary file with a specific process
            arrays = pool.map(read_binary_file, file_names)
        
        edgelist = np.concatenate(arrays, axis=0)
        arrays = [] ## release memory.
        
        edge_len = len(edgelist)
        vertex_mem_access_number_512 = 0 ## 512 bit width memory access number
        last_item = -1

        for i in range(0, edge_len, Granularity):
            batch_edges = edgelist[i:i+Granularity]
            batch_sources = batch_edges[:, 0]  # Extract source vertices from batch
            
            batch_sources //= (Granularity * BURST_LENGTH) ## coalescing and merge
            unique_values = np.unique(batch_sources) ## merge same item

            for ele in unique_values:
                if (last_item != ele):
                    vertex_mem_access_number_512 += (1 * BURST_LENGTH) ## caculate the memory operation
                    last_item = ele
        
        v_op_num[partition] = vertex_mem_access_number_512
        e_op_num[partition] = (edge_len // Granularity * 2)

    print ("v_op = ", v_op_num)
    print ("e_op = ", e_op_num)
    v_e_vloume = np.array([a / b for a, b in zip(v_op_num, e_op_num)], dtype=np.float64)
    print ("v/e volume = ", v_e_vloume)
    np.savetxt("./m_access/" + dataset + "_v_op.txt", v_op_num, fmt='%d', delimiter=' ')
    np.savetxt("./m_access/" + dataset + "_e_op.txt", e_op_num, fmt='%d', delimiter=' ')
    np.savetxt("./m_access/" + dataset + "_v_e_ratio.txt", v_e_vloume, delimiter=' ')
