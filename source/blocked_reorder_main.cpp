#include <cstdint>
#include <cstring>
#include <iostream>
#include <chrono>
#include <random>

#include "blocked_reorder.hpp"

#ifdef MEMCPY
#define blocked_reorder blocked_reorder_memcpy
#endif

#ifdef NAIVE
#define blocked_reorder blocked_reorder_naive
#endif


uint64_t run(
    uint32_t const num_blocks,
    uint32_t const floats_per_block)
{
    uint32_t const num_floats = num_blocks * floats_per_block;
    // std::cout << "num_blocks: " << num_blocks << " "
    //           << "floats_per_block: " << floats_per_block << " "
    //           << "num_floats: " << num_floats << " "
    //           << "num_bytes: " << num_floats * sizeof(float) << "\n";

    float* src = new float[num_floats];
    float* dst = new float[num_floats];
    uint32_t* src_block_starts = new uint32_t[num_blocks];
    uint32_t* dst_block_starts = new uint32_t[num_blocks];
    uint32_t* block_sizes = new uint32_t[num_blocks];

    for (uint32_t i = 0; i < num_floats; ++i) {
        float v = static_cast<float>(i);
        src[i] = v;
        dst[i] = -1.0f;
    }

    for (uint32_t i = 0; i < num_blocks; ++i) {
        src_block_starts[i] = i * floats_per_block;
        dst_block_starts[i] = i * floats_per_block;
        block_sizes[i] = floats_per_block;
    }


    std::default_random_engine noise(std::random_device{}());
    std::uniform_int_distribution<uint32_t> uniform(0, num_blocks-1);
    auto random_block = [&noise, &uniform]() -> uint32_t {
        return uniform(noise);
    };


    uint64_t duration_reorder = 0ull;

    auto global_start = std::chrono::steady_clock::now();
    for (uint32_t iteration = 0; iteration < 5; ++iteration) {
        for (uint32_t i = 0; i < num_blocks; ++i) {
            std::swap(src_block_starts[random_block()], src_block_starts[random_block()]);
            std::swap(dst_block_starts[random_block()], dst_block_starts[random_block()]);
        }

        auto reorder_start = std::chrono::steady_clock::now();
        blocked_reorder(
            src, dst,
            src_block_starts, dst_block_starts,
            block_sizes, num_blocks);
        auto reorder_stop = std::chrono::steady_clock::now();
        duration_reorder += std::chrono::duration_cast<std::chrono::nanoseconds>(reorder_stop - reorder_start).count();
    }

    delete [] src;
    delete [] dst;
    delete [] src_block_starts;
    delete [] dst_block_starts;
    delete [] block_sizes;

    return duration_reorder;
}


int main()
{
    uint32_t const floats_per_block = 30000;
    uint32_t const max_num_blocks = 1 << 15;
    for (uint32_t num_blocks = 1; num_blocks <= max_num_blocks; num_blocks <<= 1) {
        for (uint32_t sample = 0; sample < 10; ++sample) {
            uint64_t nanoseconds = run(num_blocks, floats_per_block);
            std::cout << num_blocks << " " << (double)nanoseconds / (double)std::chrono::nanoseconds::period::den << '\n';
        }
    }
}


/*
The output, if sent to suitable named files, can be plotted in Gnuplot using

gnuplot -persist -e "set xlabel 'Number of blocks' ; \
                     set ylabel 'Time [s]' ; \
                     set logscale xy 2 ; \
                     set title 'Blocked reorder' ; \
                     set key top left ; \
                     set grid ; \
                     plot 'naive_no-strict.dat' w p t 'naive no strict', \
                          'memcpy_no-strict.dat' w p t 'memcpy no strict', \
                          'naive_strict.dat' w p t 'naive strict', \
                          'memcpy_strict.dat' w p t 'memcpy strict'\
                    "

*/