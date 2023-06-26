#include <cstdint>
#include "compute_kernel_api.h"

using std::uint32_t;

// matmul C=A*B using dims MK*KN = MN (row major order)
//
namespace NAMESPACE {
void MAIN {

    constexpr int onetile = 1;

    int dst_tile_index = 0;
    int in0_block_tile_index = 0;

    uint32_t batch = get_compile_time_arg_val(0);
    uint32_t Mt = get_compile_time_arg_val(1);
    uint32_t Kt = get_compile_time_arg_val(2);
    uint32_t Nt = get_compile_time_arg_val(3);

    mm_init();

    // the simplest possible version of outer product blocked matmul
    // the reader is expected to read the A's and B's tile rows and tile columns for each output tile
    for (uint32_t nb = 0; nb < batch; nb++)
    for (uint32_t mt_C = 0; mt_C < Mt; ++mt_C) // output tile of C
    for (uint32_t nt_C = 0; nt_C < Nt; ++nt_C) // output tile index of C
    {
        acquire_dst(DstMode::Full);
        for (uint32_t kt = 0; kt < Kt; kt++) {
            cb_wait_front(CB::c_in0, onetile);
            cb_wait_front(CB::c_in1, onetile);

            matmul_tiles(CB::c_in0, CB::c_in1, 0, 0, 0, false);

            cb_pop_front(CB::c_in0, onetile);
            cb_pop_front(CB::c_in1, onetile);
        }

        cb_reserve_back(CB::c_out0, onetile);
        pack_tile(0, CB::c_out0);
        cb_push_back(CB::c_out0, onetile);

        release_dst(DstMode::Full);
    }


}
} // NAMESPACE
