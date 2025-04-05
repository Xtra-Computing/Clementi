#ifndef __HELPER_H__
#define __HELPER_H__

#define TRACE()  {printf("[TRACE] %s@%d \n", __FUNCTION__, __LINE__); fflush(stdout);}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) sizeof(arr)/sizeof((arr)[0])
#endif

#ifndef FLAG_SET
#define FLAG_SET                (1u)
#endif

#ifndef FLAG_RESET
#define FLAG_RESET              (0u)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) sizeof(arr)/sizeof((arr)[0])
#endif


#ifndef SIZE_ALIGNMENT
inline long unsigned int get_aligned_size(unsigned int in, unsigned int align)
{
    if (in == 0)
    {
        return align;
    }
    else
    {
        return (((((in - 1) / align) + 1) * align));
    }
}
#define SIZE_ALIGNMENT(in,align)    get_aligned_size((unsigned int)in,(unsigned int)align)
#endif


double getCurrentTimestamp(void);


#include <random>

template<typename T> static void shuffle(T* src, uint32_t n, bool debug_mode = true) {
    std::random_device rd;
    std::mt19937 rng(debug_mode ? 0 : rd());
    std::shuffle(src, src + n, rng);
}

#endif /* __HELPER_H__ */
