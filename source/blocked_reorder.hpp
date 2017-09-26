
void blocked_reorder_naive(
    float* src,
    float* dst,
    uint32_t* src_block_starts,
    uint32_t* dst_block_starts,
    uint32_t* block_sizes,
    uint32_t num_blocks);


void blocked_reorder_memcpy(
    float* src,
    float* dst,
    uint32_t* src_block_starts,
    uint32_t* dst_block_starts,
    uint32_t* block_sizes,
    uint32_t num_blocks);
