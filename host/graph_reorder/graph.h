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

class Vertex {
    public:
        int idx;
        int inDeg;
        int outDeg;

        std::vector<int> inVid;
        std::vector<int> outVid;

        explicit Vertex(int _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        int vertexNum;
        int edgeNum;
        std::vector<Vertex*> vertices; 

        std::vector<int> outDeg;

        std::vector<size_t> map;

        std::vector<size_t> remap;

        Graph(const std::string &fName);
        ~Graph(){
			for(int i = 0; i < vertexNum; i++){
				delete vertices[i];
			}
		};
        void getRandomStartIndices(std::vector<int> &startIndices);
        void getStat();

    private:
        bool isUgraph;
        int getMaxIdx(const std::vector<std::vector<int>> &data);
        int getMinIdx(const std::vector<std::vector<int>> &data);
        void loadFile(
				const std::string& fName,
                std::vector<std::vector<int>> &data
				);

};

class CSR{
    public:
		const int vertexNum;
		int edgeNum;
        std::vector<int> rpao; 
        std::vector<int> ciao;
        std::vector<int> rpai;
        std::vector<int> ciai;
		std::vector<int> eProps;

        // The CSR is constructed based on the simple graph
        explicit CSR(const Graph &g);
        int save2File(const std::string & fName);
		~CSR();
};

Graph* createGraph(const std::string &gName, const std::string &mode);


#endif
