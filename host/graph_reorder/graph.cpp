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
    std::vector<std::vector<int>> &data
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
            std::vector<int>(std::istream_iterator<int>(iss),
                             std::istream_iterator<int>())
        );
    }
    fhandle.close();

    std::cout << "Graph " << gName << " is loaded." << std::endl;
}

int Graph::getMaxIdx(const std::vector<std::vector<int>> &data) {
    int maxIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (maxIdx <= (*it2)) {
                maxIdx = *it2;
            }
        }
    }
    return maxIdx;
}

int Graph::getMinIdx(const std::vector<std::vector<int>> &data) {
    int minIdx = data[0][0];
    for (auto it1 = data.begin(); it1 != data.end(); it1++) {
        for (auto it2 = it1->begin(); it2 != it1->end(); it2++) {
            if (minIdx >= (*it2)) {
                minIdx = *it2;
            }
        }
    }
    return minIdx;
}

Graph::Graph(const std::string& gName) {

    // Check if it is undirectional graph
    auto found = gName.find("ungraph", 0);
    if (found != std::string::npos)
        isUgraph = true;
    else
        isUgraph = true;

    std::vector<std::vector<int>> data;
    loadFile(gName, data);
    vertexNum = getMaxIdx(data) + 1;
    edgeNum = (int)data.size();
    std::cout << "vertex num: " << vertexNum << std::endl;
    std::cout << "edge num: " << edgeNum << std::endl;

    for (int i = 0; i < vertexNum; i++) {
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for (auto it = data.begin(); it != data.end(); it++) {
        int srcIdx = (*it)[0];
        int dstIdx = (*it)[1];
        vertices[srcIdx]->outVid.push_back(dstIdx);
        vertices[dstIdx]->inVid.push_back(srcIdx);
        if (isUgraph && srcIdx != dstIdx) {
            vertices[dstIdx]->outVid.push_back(srcIdx);
            vertices[srcIdx]->inVid.push_back(dstIdx);
        }
    }

    for (auto it = vertices.begin(); it != vertices.end(); it++) {
        (*it)->inDeg = (int)(*it)->inVid.size();
        (*it)->outDeg = (int)(*it)->outVid.size();
        outDeg.push_back((*it)->outDeg);
    }

#if 1
    // map   --> new index to old index
    // remap --> old index to new index
    map = sort_indexes(outDeg);
    std::vector<size_t> remap_index(map.size());


    for (long unsigned int i = 0; i < map.size(); i ++){
        remap_index[map[i]] = i;
    //    printf("%ld %d\n", map[i], outDeg[map[i]]);
    }

    std::copy(remap_index.begin(), remap_index.end(), back_inserter(remap));
#endif

}

CSR::CSR(const Graph &g) : vertexNum(g.vertexNum), edgeNum(g.edgeNum) {
    rpao.resize(vertexNum + 1);
    rpai.resize(vertexNum + 1);
    rpao[0] = 0;
    rpai[0] = 0;

    // sort the input and output vertex
    for (int i = 0; i < vertexNum; i++) {
        int index = g.map[i];

        {
            std::sort(g.vertices[index]->outVid.begin(), g.vertices[index]->outVid.end());
            auto last = std::unique(g.vertices[index]->outVid.begin(), g.vertices[index]->outVid.end());
            g.vertices[index]->outVid.erase(last, g.vertices[index]->outVid.end());
        }
        {
            std::sort(g.vertices[index]->inVid.begin(), g.vertices[index]->inVid.end());
            auto last = std::unique(g.vertices[index]->inVid.begin(), g.vertices[index]->inVid.end());
            g.vertices[index]->inVid.erase(last, g.vertices[index]->inVid.end());
        }

        rpao[i + 1] = rpao[i] + g.vertices[index]->outVid.size();
        rpai[i + 1] = rpai[i] + g.vertices[index]->inVid.size();

        for (auto id : g.vertices[index]->outVid) {
            ciao.push_back(g.remap[id]);
        }
        for (auto id : g.vertices[index]->inVid) {
            ciai.push_back(g.remap[id]);
        }
    }
    edgeNum = ciao.size() + ciai.size();

    std::cout << "edge num: " << edgeNum/4 << std::endl;

}

int CSR::save2File(const std::string &fName)
{
    std::ostringstream command;
    command << "mkdir -p ";
    command <<"csr/";
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
    gptr = new Graph(gName);
    return gptr;
}