CXX=/opt/intel/bin/icpc
CXXFLAGS=-std=c++17 -march=native -O3 -qopt-zmm-usage=high -qopenmp -qopenmp-simd -mcmodel=medium -pthread -fopenmp -tbb -save-temps -qopt-report=5 -qopt-report-phase=loop,vec,par

SRC=singlethread*.cc helper_functions.cc
#SRC=singlethread_avx512_horizontal.cc helper_functions.cc

all: test benchmark_novec benchmark

test:
	$(CXX) $(CXXFLAGS) my_test.cc $(SRC) -o my_test.exe

run_test: test
	./prefix_sum_test.exe

benchmark:
	$(CXX) $(CXXFLAGS) prefix_sum_benchmark.cc $(SRC) -o prefix_sum_benchmark.exe

benchmark_novec:
	$(CXX) -no-vec $(CXXFLAGS) prefix_sum_benchmark.cc $(SRC) -o prefix_sum_benchmark_novec.exe

clean:
	rm -f *.i *.s *.o *.exe *.optrpt
