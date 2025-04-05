#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <vector>
#include <fstream>
#include <iostream>


#include "graph.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <in graph> " << "<out graph>" << std::endl;
        return EXIT_FAILURE;
    }
    printf("starting \n");
    fflush(stdout);

    std::string inGraph = argv[1];
    std::string outGraph = argv[2];

    Graph* gptr;
    std::string dir;
    gptr = new Graph(inGraph,true);

    CSR* csr    = new CSR(*gptr);
    //csr->save2File("graph.csr");
    std::ofstream outfile;
    outfile.open(outGraph.c_str());
    uint32_t vertex_num = csr->vertexNum;
    uint32_t edge_num   = csr->edgeNum;

    outfile << vertex_num <<" "<< edge_num << std::endl;
    int size = 0;
    for (size_t i = 1; i < vertex_num + 1 ; i++) {
        size += csr->rpao[i + 1] -  csr->rpao[i];
        for (size_t j = csr->rpao[i]; j < csr->rpao[i + 1]; j++)
        {
            outfile << csr->ciao[j] << " ";
        }
        outfile << std::endl;
    }
    outfile.flush();
    outfile.close();
    printf("size %d \n", size);
    free(gptr);
    free(csr);

    return 0;
}
