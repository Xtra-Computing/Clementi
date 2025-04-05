// ========== C++ version for input-aware partition method. ==========
// [Serial] read input edge list. 
// [Parallel] using multiply thread to divide edge list into several intervals.
// [Parallel] sort each intervals by their source index. [max 64 threads]
// [Parallel] partition into blocks. [4 SLRs].
//    step 1. divide each interval into groups.
//    step 2. greedy assign into blocks.
//    step 3. using cost model to check the condition. // if balance enough , exit.
// [Parallel] write each file into binary format.

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <numeric>
#include <utility>
#include <string>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

const int TARGET_RANGE = 1024*1024; // Range size for target vertex IDs, 1M vertices
const int numThreads = 32; // using multi-thread
const int numSLRs = 4; // equals to the number of memory channels
const int blockSize = 512; // group size, equals to burst length

std::mutex mtx; // Mutex for thread-safe output
std::atomic<int> nextSublist(0); // Atomic variable to keep track of the next sublist to process

// Function to parse the edge list file
std::vector<std::pair<int, int>> parseEdgeList(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::pair<int, int>> edgeList;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int source, target;
        if (!(iss >> source >> target)) {
            std::cerr << "Error parsing line: " << line << std::endl;
            continue;
        }
        edgeList.emplace_back(source, target);
    }
    return edgeList;
}

// Function to divide a sublist into intervals based on target vertex ID range
std::vector<std::vector<std::pair<int, int>>> divideSublist(const std::vector<std::pair<int, int>>& sublist, 
                                                            int numIntervals) {
    std::vector<std::vector<std::pair<int, int>>> intervals;
    intervals.resize(numIntervals);

    for (const auto& edge : sublist) {
        int intervalIndex = edge.second / TARGET_RANGE;
        intervals[intervalIndex].push_back(edge);
    }

    return intervals;
}

// Function executed by each thread to divide the edge list into sublists
void divideEdgeListThread (const std::vector<std::pair<int, int>>& edgeList, 
                            std::vector<std::vector<std::vector<std::pair<int, int>>>>& sublists, 
                            int intervalNum) {
    while (true) {
        int index = nextSublist.fetch_add(1);
        if (index >= sublists.size()) break; // All sublists processed

        int edgesPerThread = edgeList.size() / sublists.size();
        auto start = edgeList.begin() + index * edgesPerThread;
        auto end = (index == sublists.size() - 1) ? edgeList.end() : start + edgesPerThread;
        std::vector<std::pair<int, int>> sublist(start, end);
        sublists[index] = divideSublist(sublist, intervalNum);
    }
}

// Function to save mergedResults to a file
void saveMergedResults(const std::vector<std::vector<std::pair<int, int>>>& mergedResults) {
    std::cout << "Merged results have been saved to mergedResults.txt" << std::endl;
    std::ofstream outFile("mergedResults.txt");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for writing." << std::endl;
        return;
    }
    for (size_t group = 0; group < mergedResults.size(); ++group) {
        outFile << "Interval " << group + 1 << ":\n";
        for (const auto& interval : mergedResults[group]) {
            outFile << interval.first << " " << interval.second << "\n";
        }
    }
    outFile.close(); // Close the file when done
    std::cout << " done" << std::endl;
}

// Function to sort edge list based on the first element
void sortVector(std::vector<std::pair<int, int>>& vec) {
    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
}

// Function to divide edgelist into multiple blocks according to the value range of the source vertex
std::vector<std::vector<std::pair<int, int>>> processBlocks(std::vector<std::pair<int, int>>& sortedEdges,
                                                            float alpha) {
    std::vector<std::vector<std::pair<int, int>>> blocks;
    for (const auto& edge : sortedEdges) {
        int index = edge.first / blockSize;
        if (blocks.size() <= index) {
            blocks.resize(index + 1);
        }
        blocks[index].push_back(edge);
    }
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const auto& block)
                { return block.empty(); }), blocks.end());
    
    std::vector<std::pair<size_t, size_t>> blockSizes; // Pair of block size and original index.
    for (size_t i = 0; i < blocks.size(); ++i) {
        blockSizes.emplace_back(blocks[i].size(), i);
    }
    std::sort(blockSizes.rbegin(), blockSizes.rend());

    size_t numBlocksToSplit = std::ceil(blockSizes.size() * alpha);

    for (size_t i = 0; i < numBlocksToSplit; ++i) {
        auto& block = blocks[blockSizes[i].second];
        size_t blockSize = block.size();
        size_t splitSize = blockSize / numSLRs;
        size_t remainder = blockSize % numSLRs;

        std::vector<std::vector<std::pair<int, int>>> newSmallerBlocks;
        for (int j = 0; j < numSLRs; ++j) {
            size_t startIdx = j * splitSize + std::min(static_cast<size_t>(j), remainder);
            size_t endIdx = startIdx + splitSize + (j < remainder ? 1 : 0);
            newSmallerBlocks.push_back(std::vector<std::pair<int, int>>(block.begin() + startIdx, block.begin() + endIdx));
        }

        // Replace the original block with the new smaller blocks in the 'blocks' vector.
        blocks.erase(blocks.begin() + blockSizes[i].second);
        blocks.insert(blocks.end(), newSmallerBlocks.begin(), newSmallerBlocks.end());
    }

    return blocks;
}

// Function to use the greedy algorithm to allocate all blocks
std::vector<std::vector<std::vector<std::pair<int, int>>>> allocateBlocksToLists(
                                                std::vector<std::vector<std::pair<int, int>>>& subblocks) {
    // Sort subblocks by descending edge count to prioritize larger subblocks
    std::sort(subblocks.begin(), subblocks.end(), 
                [](const std::vector<std::pair<int, int>>& a, const std::vector<std::pair<int, int>>& b) {
        return a.size() > b.size();
    });

    std::vector<std::vector<std::vector<std::pair<int, int>>>> lists(numSLRs);
    std::vector<int> listEdgeCounts(numSLRs, 0);

    // Process subblocks in batches of numSLRs
    for (size_t i = 0; i < subblocks.size(); i += numSLRs) {
        // Sort current batch by descending edge count (if less than numSLRs left, sort those)
        std::vector<std::pair<size_t, int>> batch; // Pair<index in subblocks, edge count>
        for (size_t j = i; j < i + numSLRs && j < subblocks.size(); ++j) {
            batch.emplace_back(j, subblocks[j].size());
        }
        std::sort(batch.begin(), batch.end(), [](const std::pair<size_t, int>& a, const std::pair<size_t, int>& b) {
            return a.second > b.second; // Sort by edge count descending
        });

        // Distribute sorted batch to lists, aiming for equal total edge count
        for (auto& [index, edgeCount] : batch) {
            // Find the list with the minimal current total edge count
            auto minIt = std::min_element(listEdgeCounts.begin(), listEdgeCounts.end());
            int minIndex = std::distance(listEdgeCounts.begin(), minIt);

            lists[minIndex].push_back(std::move(subblocks[index]));
            *minIt += edgeCount; // Update the edge count for the chosen list
        }
    }

    // Clear the original subblocks vector as it's no longer needed
    std::vector<std::vector<std::pair<int, int>>>().swap(subblocks);
    return lists;
}

bool perfmodel(const std::vector<std::vector<std::vector<std::pair<int, int>>>>& lists) {
    std::vector<int> totalEdgesPerList(lists.size(), 0); // Vector to store total edges per list
    for (size_t i = 0; i < lists.size(); ++i) {
        for (const auto& block : lists[i]) {
            totalEdgesPerList[i] += block.size();
        }
    }

    bool converge = false;
    std::vector<float> perfCost(lists.size(), 0); // time cost
    // calculate the cost by performance cost
    for (size_t i = 0; i < lists.size(); ++i) {
        int blockNum = lists[i].size();
        int edgeNum = totalEdgesPerList[i];
        perfCost[i] = edgeNum * 100 + blockNum * 20;
    }

    float average = std::accumulate(perfCost.begin(), perfCost.end(), 0) / perfCost.size();
    // Find the max and min elements
    float maxCost = *std::max_element(perfCost.begin(), perfCost.end());
    float minCost = *std::min_element(perfCost.begin(), perfCost.end());

    if ((maxCost - minCost) < average * 0.05) { // if the workload imbalance not exceed 5%;
        converge = true;
    }

    return converge;
}

void saveFile(const std::vector<std::vector<std::vector<std::pair<int, int>>>>& lists, int partId) {
    auto duration = std::chrono::seconds(0);
    for (size_t i = 0; i < lists.size(); ++i) {
        std::vector<std::pair<int, int>> mergedSubblock;
        for (const auto& subblock : lists[i]) {
            mergedSubblock.insert(mergedSubblock.end(), subblock.begin(), subblock.end());
        }

        std::sort(mergedSubblock.begin(), mergedSubblock.end(), 
            [](const std::pair<int, int>& aa, const std::pair<int, int>& bb) {return aa.first < bb.first;});
        
        // Ensure alignment with 16 by adding padding edges [a, b]
        int a = mergedSubblock.empty() ? 0 : mergedSubblock.back().first;
        int num = (16 - mergedSubblock.size() % 16) + 16;
        for (int i = 0; i < num; i++) {
            mergedSubblock.push_back({a, -2});
        }

        // Prepend edge [c, c] where c is the size including padding
        size_t c = mergedSubblock.size();
        mergedSubblock.insert(mergedSubblock.begin(), {static_cast<int>(c), static_cast<int>(c)});

        auto start_write = std::chrono::high_resolution_clock::now();

        // write to Txt file
        std::string txtFilename = "p_" + std::to_string(partId) + "_sp_" + std::to_string(i) + ".txt";
        std::ofstream txtFile(txtFilename);
        if (!txtFile.is_open()) {
            std::cerr << "Failed to open " << txtFilename << std::endl;
            continue;
        }
        for (const auto& edge : mergedSubblock) {
            txtFile << edge.first << " " << edge.second << std::endl;
        }
        txtFile.close();

        // Binary file
        std::string binFilename = "p_" + std::to_string(partId) + "_sp_" + std::to_string(i) + ".bin";
        std::ofstream binFile(binFilename, std::ios::binary);
        if (!binFile.is_open()) {
            std::cerr << "Failed to open " << binFilename << std::endl;
            continue;
        }
        for (const auto& edge : mergedSubblock) {
            binFile.write(reinterpret_cast<const char*>(&edge.first), sizeof(edge.first));
            binFile.write(reinterpret_cast<const char*>(&edge.second), sizeof(edge.second));
        }
        binFile.close();

        auto end_write = std::chrono::high_resolution_clock::now();
        duration += std::chrono::duration_cast<std::chrono::seconds>(end_write - start_write);
    }
    std::cout << "thread " << partId << " save file time: " << duration.count() << " seconds" << std::endl;
}


int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file_name>\n";
        return 1;
    }
    std::string filename = argv[1];

    std::cout << "Loading " << filename << " into main memory " << std::endl;
    auto start_load = std::chrono::high_resolution_clock::now();

    std::vector<std::pair<int, int>> edgeList = parseEdgeList(filename);
    int maxVertexId = 0;
    for (const auto& edge : edgeList) {maxVertexId = std::max({maxVertexId, edge.first, edge.second});}
    int numberOfEdges = edgeList.size();
    std::cout << "Vertex range: 0 - " << maxVertexId << "; edge number = " << numberOfEdges << std::endl;

    auto end_load = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_load - start_load);
    std::cout << "load edgelist time: " << duration.count() << " seconds" << std::endl;

    auto start_process = std::chrono::high_resolution_clock::now();

    // [TODO] add remapping and outdeg generation.

    std::cout << "Dividing the edge list into sublists using " << numThreads << " threads" << std::endl;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::vector<std::pair<int, int>>>> sublists(numThreads);
    int intervalNum = (maxVertexId + TARGET_RANGE - 1) / TARGET_RANGE; // equals to subgraph number
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(divideEdgeListThread, std::cref(edgeList), std::ref(sublists), intervalNum);
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::vector<std::pair<int, int>>().swap(edgeList); // realease the memory.

    std::vector<std::vector<std::pair<int, int>>> mergedResults(intervalNum); // merge results into single list.
    for (size_t group = 0; group < intervalNum; ++group) {
        for (const auto& threadList : sublists) {
            if (group < threadList.size()) { // Check if the current group exists in the thread
                for (const auto& interval : threadList[group]) {mergedResults[group].push_back(interval);}
            }
        }
    }
    std::vector<std::vector<std::vector<std::pair<int, int>>>>().swap(sublists); // realease the memory. 

    std::cout << "Process data using " << intervalNum << " threads" << std::endl;
    std::vector<std::thread> sortThread;
    for (int i = 0; i < intervalNum; ++i) {
        sortThread.emplace_back([&mergedResults, i]() {
            std::cout << "thread " << i << " sort begin" << std::endl;
            sortVector(mergedResults[i]);
            std::cout << "thread " << i << " sort end" << std::endl;
            float alpha = 0.2; // first set as 20% top blocks
            while (true) {
                auto blocks = processBlocks(mergedResults[i], alpha);
                auto lists = allocateBlocksToLists(blocks);
                bool converge = perfmodel(lists);
                std::cout << "thread " << i << " alpha " << alpha << " converge " << converge << std::endl; 
                if (converge == false) {
                    alpha = alpha + 0.02;
                } else {
                    std::cout << "thread " << i << " finish, save file" << std::endl;
                    saveFile(lists, i);
                    break;
                }
            }
        });
    }
    for (auto& thread : sortThread) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    auto end_process = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::seconds>(end_process - start_load);
    std::cout << "process time: " << duration.count() << " seconds" << std::endl;

    return 0;
}
