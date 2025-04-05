#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


typedef uint16_t mask_t;

unsigned int count_set_bits(unsigned int n)
{
    unsigned int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

static uint32_t sum_bit_counts = 0;
static uint32_t golden_sum_bit_counts = 0;
static uint32_t sum_mask =0;
static uint32_t golden_sum_mask = 0;

void clear_mask(void)
{
    sum_bit_counts = 0;
    golden_sum_bit_counts = 0;
    sum_mask =0;
    golden_sum_mask = 0;
}
void write_out_mask(mask_t mask)
{
    sum_bit_counts += count_set_bits(mask);
    sum_mask += mask;
    printf("%04x ", mask);
}

void golden_write_out_mask(mask_t mask)
{
    golden_sum_bit_counts += count_set_bits(mask);
    golden_sum_mask += mask;
}

uint32_t match_mask(void)
{
    printf("total bits %d\n", sum_bit_counts);
    if (golden_sum_bit_counts != sum_bit_counts){
        printf("ERROR! golden mismatch\n");
        exit(-2);
    }
    if (golden_sum_mask != sum_mask){
        printf("ERROR! golden mask mismatch\n");
        exit(-3);
    }
    return sum_bit_counts;
}




const mask_t mask_lut[16] = {
    0x0000, 0x0001, 0x0003, 0x0007,
    0x000f, 0x001f, 0x003f, 0x007f,
    0x00ff, 0x01ff, 0x03ff, 0x07ff,
    0x0fff, 0x1fff, 0x3fff, 0x7fff
};

const mask_t inv_mask_lut[16] = {
    0x0001, 0x0003, 0x0007, 0x000f,
    0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff,
    0x1fff, 0x3fff, 0x7fff, 0xffff
};


//                  inclusive        inclusive
mask_t gen_mask_in_16(uint32_t start, uint32_t end)
{
    uint8_t s = start & 0xf;
    uint8_t e = end & 0xf;
    mask_t s_mask = ~mask_lut[s];
    mask_t e_mask = inv_mask_lut[e];
    return s_mask & e_mask;
}


//                inclusive  exclusive
void golden_mask_output(int start, int end)
{

    uint32_t start_addr = start & 0xfffffff0;
    uint32_t curr_e_addr = start_addr;
    uint32_t temp = start;
    uint32_t inclusive_end = end - 1;
    while(1)
    {
        curr_e_addr += 16;
        uint32_t s = temp;
        uint32_t e = curr_e_addr - 1 ;

        if (e > inclusive_end)
        {
            e = inclusive_end;
        }

        golden_write_out_mask(gen_mask_in_16(s,e));

        temp += (e - s + 1);

        if (e == inclusive_end)
        {
            break;
        }
    }
}


//                inclusive  exclusive
void mask_output(int start, int end)
{

    uint32_t start_addr = start & 0xfffffff0;
    uint32_t curr_e_addr = start_addr;
    uint32_t temp = start;
    uint32_t inclusive_end = end - 1;

    uint32_t temp_end_addr = end & 0xfffffff0;
    uint32_t end_addr;
    if (temp_end_addr == end)
    {
        end_addr = temp_end_addr;
    }
    else
    {
        end_addr =  temp_end_addr + 16;
    }

    uint32_t curr_e_in = start_addr - 1 + 16;

    while(1)
    {
        curr_e_addr += 16;
        uint32_t s = temp;

        if (curr_e_in > inclusive_end)
        {
            curr_e_in = inclusive_end;
        }

        write_out_mask(gen_mask_in_16(s,curr_e_in));

        temp += (curr_e_addr - s);
        curr_e_in += 16;

        if (curr_e_addr == end_addr)
        {
            break;
        }
    }
    printf("\n");
}



void burst_read(int addr, int len)
{
    printf("burst %d %d\n", addr, len);
}

//size should not be zero
void burst_gen(int start, int size)
{

    uint32_t start_addr = start & 0xfffffff0;
    uint32_t end = start + size;
    uint32_t temp_end_addr = end & 0xfffffff0;

    uint32_t end_addr  = 0;
    if (temp_end_addr == end)
    {
        end_addr = temp_end_addr;
    }
    else
    {
        end_addr =  temp_end_addr + 16;
    }
    printf("#############################\n");
    printf("start %d, end %d\n", start, end);
    printf("align_start %d, align_end %d\n", start_addr, end_addr);
    uint32_t curr_addr = start_addr;
    while (curr_addr != end_addr)
    {
        uint32_t last = curr_addr;
        if (end_addr >= (curr_addr + 32 * 16))
        {
            burst_read(curr_addr, 32);
            curr_addr += 32 * 16;
        }
        else
        {
            burst_read(curr_addr, 1);
            curr_addr += 16;
        }

        mask_output((last < start?(start): (last)),
                  ((curr_addr > end)?(end):(curr_addr)));
        golden_mask_output((last < start?(start): (last)),
                  ((curr_addr > end)?(end):(curr_addr)));
    }
}

void test(int start, int size)
{
    clear_mask();

    burst_gen(start,size);
    uint32_t res = match_mask();
    if ( res != size)
    {
        printf("ERROR!\n");
        exit(-1);
    }
    else
    {
        printf("SUCCESS\n");
    }
}
int main(int argc, char **argv)
{
    printf("test start\n");
    uint32_t addr = 16;
    uint32_t j = 4;
    uint32_t a = addr + (j << 4);
    uint32_t b = addr + j << 4;
    printf("a %d b %d \n",a ,b);

    return 0;
}