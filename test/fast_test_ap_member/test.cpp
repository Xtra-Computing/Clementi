#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


#define offset_of(st, m)            ((const size_t)((char *)&((st *)0)->m - (char *)0))
#define member_size(type, member)   ((const size_t)(sizeof(((type *)0)->member)))
#define member_dim(type, member)    ((const size_t)(sizeof(((type*)0)->member) / \
                                    sizeof(((type*)0)->member[0])))

#define member_start_bit(st,m)          (offset_of(st,m) * 8)
#define member_end_bit(st,m)            ((offset_of(st,m) + member_size(st,m)) * 8 - 1)

#define member_unit_size(st,m)          ((const size_t)(member_size(st,m)/member_dim(st,m)))
#define member_array_start_bit(st,m,i)  ((const size_t)(member_start_bit(st,m) + (member_unit_size(st,m) * i * 8)))
#define member_array_end_bit(st,m,i)    ((const size_t)(member_start_bit(st,m) + (member_unit_size(st,m) * (i + 1) * 8) - 1))

typedef struct
{
    uint32_t a;
    uint16_t b[10];
} test_t;


int main(int argc, char **argv)
{
    printf("test start\n");
    printf("b %ld %ld %ld \n",
        member_start_bit(test_t, b),
        member_array_start_bit(test_t,b,0) ,
        member_array_end_bit(test_t,b,0));

    return 0;
}