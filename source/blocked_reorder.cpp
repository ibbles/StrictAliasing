#include <stdint.h>
#include <string.h>

void blocked_reorder_naive(
    float* src, float* dst,
    uint32_t* src_block_starts, uint32_t* dst_block_starts,
    uint32_t* block_sizes,
    uint32_t num_blocks)
{
    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        for (uint32_t elem_idx = 0; elem_idx < block_sizes[block_idx]; ++elem_idx) {
            uint32_t src_start = src_block_starts[block_idx];
            uint32_t dst_start = dst_block_starts[block_idx];
            float f = src[src_start + elem_idx];
            dst[dst_start + elem_idx] = f;
        }
    }
}


void blocked_reorder_memcpy(
    float* src,
    float* dst,
    uint32_t* src_block_starts,
    uint32_t* dst_block_starts,
    uint32_t* block_sizes,
    uint32_t num_blocks)
{
    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        memcpy(
            &dst[dst_block_starts[block_idx]],
            &src[src_block_starts[block_idx]],
            block_sizes[block_idx] * sizeof(float));
    }
}


