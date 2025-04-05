# ========== python version for input-aware partition method. ==========

import numpy as np
import multiprocessing
import sys
import os
import psutil

SUB_PARTITION_NUM = 4
burst_len = 32
burst_vertex_num = burst_len * 16  ## 16 = 512 bit / 32 bit

base_path = "/data/yufeng/4node_4SLR_graph_dataset/"  # Replace with the actual input file path
base_sub_partition = 16
storage_path = "/data/yufeng/single_4SLR_graph_dataset_bp2/"
dataset_name = ["Friendster", "TW", "UK2007-05", "G25", "WP", "LJ"] ## for large real graph processing
# dataset_name = ["R25-2-uniform", "R25-2-1", "R25-2-2", "R25-2-3",
#                 "R25-4-0", "R25-4-1", "R25-4-2", "R25-4-3",
#                 "R25-8-0", "R25-8-1", "R25-8-2", "R25-8-3",
#                 "R25-16-0", "R25-16-1", "R25-16-2", "R25-16-3",
#                 "R25-32-0", "R25-32-1", "R25-32-2", "R25-32-3",
#                 "R25-64-0", "R25-64-1", "R25-64-2", "R25-64-3"] ## for large real graph processing

# Function to read a binary file and return its contents as a NumPy array
def read_binary_file(filename):
    with open(filename, 'rb') as file:
        data = np.fromfile(file, dtype=np.int32)
        data = data.reshape(-1, 2)
        data = data[(data[:, 1] != -2)][1:]
    return data

def process_group(i, groups, group_sum, group_index, edge_array, s_path, partition):
    print("sp =", i, " outdegree num =", group_sum[i], " vertex group num =", len(groups[i]))
    group_sum[i] = (group_sum[i] + 15) // 16 * 16 + 16  # for 16 alignment

    edge_ptt = np.empty((0, 2), dtype=np.int32)
    for group_id in groups[i]:
        lower_bound = group_index[group_id]
        upper_bound = group_index[group_id + 1]
        temp_array = edge_array[lower_bound:upper_bound]
        if len(temp_array) > 0:
            edge_ptt = np.vstack((edge_ptt, temp_array))

    sorted_idx = np.argsort(edge_ptt[:, 0])  # sort the edgelist by source vertex id
    edge_ptt = edge_ptt[sorted_idx]

    num_dummy_edges = (16 - len(edge_ptt) % 16) % 16 + 16
    last_source_vertex = edge_ptt[-1, 0]
    dummy_edges = np.array([[last_source_vertex, -2]] * num_dummy_edges, dtype=np.int32)
    edge_ptt = np.vstack((edge_ptt, dummy_edges))

    edge_ptt = np.insert(edge_ptt, 0, np.array([group_sum[i], group_sum[i]], dtype=np.int32), axis=0)

    edge_ptt.tofile(s_path + "/p_" + str(partition) + "_sp_" + str(i) + ".bin")
    np.savetxt(s_path + "/p_" + str(partition) + "_sp_" + str(i) + ".txt", edge_ptt, fmt='%d', delimiter=' ')


for dataset in dataset_name:

    file_path = base_path + dataset
    partition_num = 0
    for p in range(100):
        ck_file_name = file_path + "/p_" + str(p) + "_sp_0.bin"
        if os.path.exists(ck_file_name):
            partition_num = p + 1
    print ("dataset = ", dataset, " partition_num = ", partition_num)
    
    s_path = storage_path + dataset
    if (os.path.exists(s_path) == True):
        print("Already exists ", s_path)
        continue
    else :
        os.mkdir(s_path)
        print("Create directory ", s_path)

    for partition in range(partition_num):

        file_names = []
        for sp_idx in range(base_sub_partition):
            file_names.append(base_path + dataset + "/p_" + str(partition) + "_sp_" + str(sp_idx) + ".bin")
            
        # Create a multiprocessing Pool with base_sub_partition processes
        with multiprocessing.Pool(processes=base_sub_partition) as pool:
            # Use pool.map to read each binary file with a specific process
            arrays = pool.map(read_binary_file, file_names)
        
        edge_array = np.concatenate(arrays, axis=0)
        arrays = [] ## release memory.

        ## already finish sort: ascending order. ## important
        vertex_num = edge_array.max() + 1
        print ("Dataset = ", dataset, " p_idx = ", partition, " vertex num = ", vertex_num)

        source_nodes = edge_array[:, 0].copy()
        source_nodes //= burst_vertex_num
        source_nodes = np.array(source_nodes, dtype = np.int32)

        print (source_nodes)

        # max_group_id = np.max(source_nodes)
        # group_edge_num = np.bincount(source_nodes, minlength=max_group_id+1)
        group_edge_num = np.bincount(source_nodes) ## get the src vertex group and its corresponding outdeg.
        print (group_edge_num)
        
        # [OPTIMIZATION] divide the large outdeg item into several sub-items.
        dividing_factor = SUB_PARTITION_NUM
        ## threshold = np.max(group_edge_num) * 0.1
        ## we choose the max value * 0.8, to aviod the uniform case, add np.mean value.
        threshold = np.maximum(np.max(group_edge_num) * 0.8, (np.mean(group_edge_num) * 2)) ## exceed threshold, divide by dividing_factor.
        ## threshold = np.maximum(np.percentile(group_edge_num, 90), (np.mean(group_edge_num) * 2)) ## exceed threshold, divide by dividing_factor.
        print ("max_outdeg = ", np.max(group_edge_num), " sum_outdeg = ", np.sum(group_edge_num), " threshold = ", threshold)
        result_list = []
        for group_item in group_edge_num:
            if (group_item > threshold):
                # print ("optimization 1")
                sub_items = np.full(dividing_factor, group_item // dividing_factor)
                remainder = group_item % dividing_factor
                sub_items[:remainder] += 1
                result_list.extend(sub_items)
            else:
                result_list.append(group_item)
        
        group_edge_num = np.array(result_list, dtype=np.int32)
        print (group_edge_num)
        
        ## get edge index for each group 
        accumulated_group = np.cumsum(group_edge_num)
        group_index = np.insert(accumulated_group, 0, 0)
        print (group_index)


        ## sort the source vertex of subgraph by indegree in a decending order
        sorted_node_ids = np.argsort(group_edge_num)[::-1]
        sorted_out_degree_values = group_edge_num[sorted_node_ids]

        combined_list = list(zip(sorted_node_ids, sorted_out_degree_values))
        groups = [[] for _ in range(SUB_PARTITION_NUM)]
        group_sum = [0] * SUB_PARTITION_NUM

        ## divide the group by SUB_PARTITION_NUM, partition method => make the vertex number in each group balanced.
        aligned_nodes = (len(sorted_node_ids) // SUB_PARTITION_NUM) * SUB_PARTITION_NUM

        for start in range(0, aligned_nodes, SUB_PARTITION_NUM):
            group_sort_indices = np.argsort(group_sum)
            end = start + SUB_PARTITION_NUM
            nodes_to_assign = combined_list[start:end]
            
            for idx, item in enumerate(nodes_to_assign):
                groups[group_sort_indices[idx]].append(item[0])
                group_sum[group_sort_indices[idx]] += item[1]

        group_sort_indices = np.argsort(group_sum)
        nodes_to_assign = combined_list[aligned_nodes:len(sorted_node_ids)] ## for the remaining edge list.

        for idx, item in enumerate(nodes_to_assign):
            groups[group_sort_indices[idx]].append(item[0])
            group_sum[group_sort_indices[idx]] += item[1]
        
        ## [OPTIMIZATION] adjust edge number to make the execution time in each group balanced. 
        # set iteration number = 100000, up_bound = 1.1 which means the max edge number not exceed (bound*avg_edge)
        iteration = 100000
        up_bound = 1.1
        avg_edge_num = sum(group_sum) // SUB_PARTITION_NUM
        for iter_num in range(iteration):
            max_edge_num = max(group_sum)
            min_edge_num = min(group_sum)
            if (max_edge_num <= (up_bound * avg_edge_num)):
                break
            else:
                max_group_id = group_sum.index(max_edge_num)
                min_group_id = group_sum.index(min_edge_num)
                last_item = groups[max_group_id].pop()
                last_item_id = np.where(sorted_node_ids == last_item)[0]
                outdeg_value = sorted_out_degree_values[last_item_id]

                former_distance = group_sum[max_group_id] - group_sum[min_group_id]
                group_sum[min_group_id] = group_sum[min_group_id] + outdeg_value
                group_sum[max_group_id] = group_sum[max_group_id] - outdeg_value
                sum_max = max(group_sum)
                sum_min = min(group_sum)

                if ((sum_max - sum_min) < (former_distance)):
                    groups[min_group_id].append(last_item)
                    print ("optimization 2")
                    # print ("move ", last_item, " from ", max_group_id, " to", min_group_id)
                else:
                    groups[max_group_id].append(last_item)
                    group_sum[min_group_id] = group_sum[min_group_id] - outdeg_value
                    group_sum[max_group_id] = group_sum[max_group_id] + outdeg_value
                    print ("iteration ", iter_num, " : finish optimization")
                    break
        
        converted_list = [item.tolist() if isinstance(item, np.ndarray) else item for item in group_sum]
        flattened_list = []
        for item in converted_list:
            if isinstance(item, list):
                flattened_list.extend(item)
            else:
                flattened_list.append(item)
        group_sum = flattened_list
        print ("After optimization, edge number in each group", group_sum)
        
        # Create a multiprocessing Pool
        with multiprocessing.Pool(processes=SUB_PARTITION_NUM) as pool:
            # Parallelize the processing of groups and edge arrays
            pool.starmap(process_group, [(i, groups, group_sum, group_index, edge_array, s_path, partition) \
                                        for i in range(SUB_PARTITION_NUM)])


