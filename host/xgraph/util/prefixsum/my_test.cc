#include <cstddef>
#include <numeric>
#include <vector>
#include <iostream>

#include "data.h"
#include "singlethread_avx.h"
#include "singlethread_avx512_horizontal.h"
#include "singlethread_avx512_vertical.h"
#include "singlethread_scalar.h"
#include "singlethread_sse.h"
#include "singlethread_tree.h"

int main(){
    float testdata[64] = {0.1,0.2,
	    1,2,3,4,5,6,7,8,9,0,
	    1,2,3,4,5,6,7,8,9,0,
	    1,2,3,4,5,6,7,8,9,0};
    float outputdata[64]={};
    PrefixSumAVX512Vertical1(32,testdata,outputdata);
    for(int i = 0; i < 32;i++)
    	std::cout << outputdata[i] << std::endl;
    return 0;
}
