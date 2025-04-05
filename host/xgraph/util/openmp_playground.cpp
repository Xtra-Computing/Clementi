//
// Created by Shixuan Sun on 2020/10/22.
//

#include <iostream>
#include <omp.h>
#include <unistd.h>

void f() {
#pragma omp parallel for
    for (int i = 0; i < 4; ++i) {
        printf("%d : %d\n", omp_get_thread_num(), i);
    }
}

static int tp;

int main() {
    omp_set_nested(1);
    omp_set_num_threads(10);
    printf("There are %d processors\n", omp_get_num_procs());
    printf("There are %d threads\n", omp_get_num_threads());
    printf("The thread ID is %d\n", omp_get_thread_num());

#pragma omp threadprivate(tp)

#pragma omp parallel num_threads(7)
    tp = omp_get_thread_num();

#pragma omp parallel num_threads(9)
    printf("Thread %d has %d\n",omp_get_thread_num(),tp);

#pragma omp parallel
    {
        int private_id = omp_get_thread_num();
        printf("Hello %d\n", omp_get_thread_num());
        printf("Hello id %d %d\n", omp_get_thread_num(), private_id);
#pragma omp for
        for (int i = 0; i < 10; ++i) {
            printf("Single %d %d %d!\n", i, omp_get_thread_num(), private_id);
        }
    };

#pragma omp parallel
#pragma omp single
    {
        int count = 100;
        while (count>0) {
#pragma omp task
            {
                int countcopy = count;
                if (count==50) {
                    sleep(1);
                    printf("%d,%d\n",count,countcopy);
                } // end if
            }   // end task
            count--;
        }     // end while
        printf("After create tasks\n");
    }       // end single

    return 0;
}