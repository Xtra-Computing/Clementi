#include "graph.h"
#include <cstdlib>
#include <iostream>
#include <fstream>




#include <iostream>
#include <vector>
#include <numeric>      // std::iota
#include <algorithm>

template <typename T>
std::vector<size_t> sort_indexes(const std::vector<T> &v) {

    // initialize original index locations
    std::vector<size_t> idx(v.size());
    std::iota(idx.begin(), idx.end(), 0);

    // sort indexes based on comparing values in v
    // using std::stable_sort instead of std::sort
    // to avoid unnecessary index re-orderings
    // when v contains elements of equal values
    std::stable_sort(idx.begin(), idx.end(),
    [&v](size_t i1, size_t i2) {return v[i1] > v[i2];});

    return idx;
}

void Graph::loadFile(
    const std::string& gName,
    std::vector<std::vector<id_t>> &data
)
{
    std::ifstream fhandle(gName.c_str());
    if (!fhandle.is_open()) {
        HERE;
        std::cout << "Failed to open " << gName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(fhandle, line)) {
        std::istringstream iss(line);
        data.push_back(
            std::vector<id_t>(std::istream_iterator<id_t>(iss),
                              std::istream_iterator<id_t>())
        );
    }
    fhandle.close();

    std::cout << "Graph " << gName << " is loaded." << std::endl;
}

id_t Graph::getMaxIdx(const std::vector<std::vector<id_t>> &data) {
    id_t maxIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (maxIdx <= (*it2)) {
                maxIdx = *it2;
            }
        }
    }
    return maxIdx;
}

id_t Graph::getMinIdx(const std::vector<std::vector<id_t>> &data) {
    id_t minIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (minIdx >= (*it2)) {
                minIdx = *it2;
            }
        }
    }
    return minIdx;
}

void Graph::readGraph(const std::string& gName, bool undirected) {
    // Check if it is undirectional graph
    auto found = gName.find("ungraph", 0);
    if (found != std::string::npos)
        isUgraph = true;
    else
        isUgraph = false;

    std::vector<std::vector<id_t>> data;
    loadFile(gName, data);
    id_t localVertexNum = getMaxIdx(data) + 1;
    vertexNum = getMaxIdx(data);
    edgeNum = (id_t)data.size();
    std::cout << "vertex num: " << vertexNum << std::endl;
    std::cout << "edge num: " << edgeNum << std::endl;

    isUgraph |= undirected;
    printf("isUgraph %d\n",isUgraph);
    {
        for (id_t i = 0; i < localVertexNum; i++) {
            Vertex* v = new Vertex(i);
            vertices.push_back(v);
        }

        for (auto it = data.begin(); it != data.end(); it++) {
            id_t srcIdx = (*it)[0];
            id_t dstIdx = (*it)[1];
            vertices[srcIdx]->outVid.push_back(dstIdx);
            vertices[dstIdx]->inVid.push_back(srcIdx);
            if (isUgraph && srcIdx != dstIdx) {
                vertices[dstIdx]->outVid.push_back(srcIdx);
                vertices[srcIdx]->inVid.push_back(dstIdx);
            }
        }
        int outsize = 0;
        int insize = 0;
        unsigned int maxout = 0;
        int maxoutid = 0;
        int itid = 0;
        for (auto it = vertices.begin(); it != vertices.end(); it++) {
            (*it)->inDeg = (id_t)(*it)->inVid.size();
            (*it)->outDeg = (id_t)(*it)->outVid.size();
            insize += (*it)->inDeg;
            outsize += (*it)->outDeg;
            outDeg.push_back((*it)->outDeg);
            if ((*it)->outDeg > maxout)
            {
                maxout = (*it)->outDeg;
                maxoutid = itid;
            }
            itid++;
        }
        printf("edge size: %d\n", insize + outsize);
        printf("edge in:   %d\n", insize);
        printf("edge out:  %d\n", outsize);
        printf("maxoutid %d \n", maxoutid);
        printf("maxout %u \n", maxout);
        map = sort_indexes(outDeg);
        std::vector<size_t> remap_index(map.size());
        //map[new_id] = ori_id
        //remap[ori_id] = new_id

        for (long unsigned int i = 0; i < map.size(); i ++) {
            remap_index[map[i]] = i;
        }
        std::copy(remap_index.begin(), remap_index.end(), back_inserter(remap));
#if 0
        for (long unsigned int i = 0; i < map.size(); i ++) {
            printf("i %ld, %ld map to %ld with degree %d, %ld remap to %ld with degree %d\n",
                   i, map[i], remap[map[i]], outDeg[map[i]],
                   remap[i], map[remap[i]], outDeg[remap[i]]);
        }
#endif
    }

    //reload with mapped
    vertices.clear();

    for (id_t i = 0; i < localVertexNum; i++) {
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for (auto it = data.begin(); it != data.end(); it++) {
        id_t src = (*it)[0];
        id_t dst = (*it)[1];
        //map[new_id] = ori_id
        //remap[ori_id] = new_id
        id_t srcIdx = remap[src];
        id_t dstIdx = remap[dst];
        vertices[srcIdx]->outVid.push_back(dstIdx);
        vertices[dstIdx]->inVid.push_back(srcIdx);
        if (isUgraph && srcIdx != dstIdx) {
            vertices[dstIdx]->outVid.push_back(srcIdx);
            vertices[srcIdx]->inVid.push_back(dstIdx);
        }
    }

    for (auto it = vertices.begin(); it != vertices.end(); it++) {
        (*it)->inDeg = (id_t)(*it)->inVid.size();
        (*it)->outDeg = (id_t)(*it)->outVid.size();
        //printf("%d \n",(*it)->inDeg);
    }


}

Graph::Graph(const std::string& gName) {
    readGraph(gName, false);
}

Graph::Graph(const std::string& gName, bool undirected) {
    readGraph(gName, undirected);
}



CSR::CSR(const Graph &g) : vertexNum(g.vertexNum), edgeNum(g.edgeNum) {
    rpao.resize(vertexNum + 1);
    rpai.resize(vertexNum + 1);
    rpao[0] = 0;
    rpai[0] = 0;
    for (id_t i = 0; i < vertexNum + 1; i++) {
        rpao[i + 1] = rpao[i] + g.vertices[i]->outDeg;
        rpai[i + 1] = rpai[i] + g.vertices[i]->inDeg;
    }
    std::cout << "index 0: " << rpao[1] << " " << rpai[1] << std::endl;

    // sort the input and output vertex
    for (id_t i = 0; i < vertexNum + 1; i++) {
        std::sort(g.vertices[i]->outVid.begin(), g.vertices[i]->outVid.end());
        std::sort(g.vertices[i]->inVid.begin(), g.vertices[i]->inVid.end());
        for (auto id : g.vertices[i]->outVid) {
            ciao.push_back(id);
        }
        for (auto id : g.vertices[i]->inVid) {
            ciai.push_back(id);
        }
    }
#if 0
    for (id_t i = 0; i < edgeNum; i++) {
        eProps.push_back(rand() % 10);
    }
#endif
}

int CSR::save2File(const std::string &fName)
{
    std::ostringstream command;
    command << "mkdir -p ";
    command << "csr/";
    command << fName;
    int ret = system(command.str().c_str());
    if (ret < 0)
    {
        return ret;
    }
    std::ostringstream offsetsName, columnsName;
    offsetsName << "csr/";
    offsetsName << fName;
    offsetsName << "/csr_offsets.txt";
    std::ofstream offset;
    offset.open(offsetsName.str().c_str());

    columnsName << "csr/";
    columnsName << fName;
    columnsName << "/csr_columns.txt";
    std::ofstream columns;
    columns.open(columnsName.str().c_str());

    offset << (rpao.size() - 1) << std::endl;
    for (auto item : rpao)
    {
        offset << item << std::endl;
    }
    offset.flush();
    offset.close();

    columns << ciao.size() << std::endl;
    for (auto item : ciao)
    {
        columns << item << std::endl;
    }
    columns.flush();
    columns.close();

    return 0;


}


Graph* createGraph(const std::string &gName, const std::string &mode) {
    Graph* gptr;
    std::string dir;
    gptr = new Graph(gName,true);
    return gptr;
}