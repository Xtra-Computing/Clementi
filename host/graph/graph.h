#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <map>
#include <algorithm>

#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

typedef uint32_t id_t;

class Vertex {
    public:
        id_t idx;
        id_t inDeg;
        id_t outDeg;

        std::vector<id_t> inVid;
        std::vector<id_t> outVid;

        explicit Vertex(id_t _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        id_t vertexNum;
        id_t edgeNum;
        std::vector<Vertex*> vertices; 

        Graph(const std::string &fName);
        Graph(const std::string &gName, bool undirected);
        ~Graph(){
			for(id_t i = 0; i < vertexNum; i++){
				delete vertices[i];
			}
		};
        void getRandomStartIndices(std::vector<id_t> &startIndices);
        void getStat();

    private:
        void readGraph(const std::string &gName, bool undirected);
        bool isUgraph;
        id_t getMaxIdx(const std::vector<std::vector<id_t>> &data);
        id_t getMinIdx(const std::vector<std::vector<id_t>> &data);
        void loadFile(
				const std::string& fName,
                std::vector<std::vector<id_t>> &data
				);

};

class CSR{
    public:
		const id_t vertexNum;
		const id_t edgeNum;
        std::vector<id_t> rpao;
        std::vector<id_t> ciao;
        std::vector<id_t> rpai;
        std::vector<id_t> ciai;
		std::vector<id_t> eProps;

        // The CSR is constructed based on the simple graph
        explicit CSR(const Graph &g);
        int save2File(const std::string & fName);
		~CSR();
        bool ischild(id_t p, id_t c);
};

Graph* createGraph(const std::string &gName, const std::string &mode);


#endif
