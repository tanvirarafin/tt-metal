// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "dataflow_api.h"


FORCE_INLINE void generate_bcast_scaler() {
    constexpr uint32_t cb_in_2 = 2;
    uint32_t scaler = get_arg_val<uint32_t>(4);
    cb_reserve_back(cb_in_2, 1);
    constexpr uint32_t num_zeros_reads = 2048 / MEM_ZEROS_SIZE;
    uint64_t zeros_noc_addr = get_noc_addr(MEM_ZEROS_BASE);
    uint32_t write_addr = get_write_ptr(cb_in_2);
    // Fill tile with zeros
    for (uint32_t i = 0; i < num_zeros_reads; ++i) {
        noc_async_read(zeros_noc_addr, write_addr, MEM_ZEROS_SIZE);
        write_addr += MEM_ZEROS_SIZE;
    }
    noc_async_read_barrier();
    volatile tt_l1_ptr uint32_t* ptr = reinterpret_cast<volatile tt_l1_ptr uint32_t*>(get_write_ptr(cb_in_2));
    uint32_t idx = 0;
    for (uint32_t k = 0; k < 4; ++k) {
        uint32_t curr_idx = idx;
        for (uint32_t j = 0; j < 8; ++j) {
            ptr[curr_idx] = scaler;
            curr_idx++;
        }
        idx += 128;
    }
    cb_push_back(cb_in_2, 1);
}

FORCE_INLINE void generate_epsilon() {
    constexpr uint32_t eps_cb_id = 3;
    union { float f; uint32_t u; } u; u.u = get_arg_val<uint32_t>(5);
    cb_reserve_back(eps_cb_id, 1);
    uint16_t eps = uint16_t(u.u>>16);
    auto ptr = reinterpret_cast<uint16_t*>(get_write_ptr(eps_cb_id));
    constexpr uint32_t num_zeros_reads = 2048 / MEM_ZEROS_SIZE;
    uint64_t zeros_noc_addr = get_noc_addr(MEM_ZEROS_BASE);
    uint32_t write_addr = get_write_ptr(eps_cb_id);
    // Fill tile with zeros
    for (uint32_t i = 0; i < num_zeros_reads; ++i) {
        noc_async_read(zeros_noc_addr, write_addr, MEM_ZEROS_SIZE);
        write_addr += MEM_ZEROS_SIZE;
    }
    noc_async_read_barrier();

    uint32_t idx = 0;
    for (int k = 0; k < 4; k+=2) {
        uint32_t curr_idx = idx;
        for (int j = 0; j < 16; ++j) {
            ptr[curr_idx] = eps;
            idx += 16;
        }
        curr_idx += 512;
    }
    cb_push_back(eps_cb_id, 1);
}


void kernel_main() {
    uint32_t src_addr  = get_arg_val<uint32_t>(0);
    uint32_t NCHt      = get_arg_val<uint32_t>(1);
    uint32_t Wt        = get_arg_val<uint32_t>(2);
    uint32_t tile_offset = get_arg_val<uint32_t>(3);

    uint32_t gamma_addr = get_arg_val<uint32_t>(6);
    uint32_t beta_addr = get_arg_val<uint32_t>(7);
    uint32_t b_addr = get_arg_val<uint32_t>(8);


    constexpr uint32_t cb_id_in0 = 0, cb_id_in1 = 1;
    constexpr uint32_t cb_id_gamma = 5;
    constexpr uint32_t cb_id_beta = 6;

    // ublocks size defined in tiles
    const uint32_t src0_tile_bytes = get_tile_size(cb_id_in0);
    const DataFormat src0_data_format = get_dataformat(cb_id_in0);

    constexpr bool src0_is_dram = get_compile_time_arg_val(0) == 1;
    constexpr bool src1_is_dram = get_compile_time_arg_val(1) == 1;
    constexpr bool gamma_is_dram = get_compile_time_arg_val(2) == 1;
    constexpr bool beta_is_dram = get_compile_time_arg_val(3) == 1;
    constexpr uint32_t blk = get_compile_time_arg_val(4); // needed for correctness of softmax/LN kernels

    const InterleavedAddrGenFast<src0_is_dram> src_a = {
        .bank_base_address = src_addr,
        .page_size = src0_tile_bytes,
        .data_format = src0_data_format
    };

    #define stick_size_is_pow2 get_compile_time_arg_val(5) == 1
    #if (stick_size_is_pow2)
    const uint32_t log_base_2_of_page_size = get_compile_time_arg_val(6);
    #else
    const uint32_t page_size = get_compile_time_arg_val(6);
    #endif
    #ifdef FUSE_GAMMA
    #if (stick_size_is_pow2)
    const InterleavedPow2AddrGen<gamma_is_dram> addrg = {
        .bank_base_address = gamma_addr,
        .log_base_2_of_page_size = log_base_2_of_page_size
    };
    #else
    const InterleavedAddrGen<gamma_is_dram> addrg = {
        .bank_base_address = gamma_addr,
        .page_size = page_size
    };
    #endif
    const uint32_t gamma_tile_bytes = get_tile_size(cb_id_gamma);
    #endif
    #ifdef FUSE_BETA
    #if (stick_size_is_pow2)
    const InterleavedPow2AddrGen<beta_is_dram> addrb = {
        .bank_base_address = beta_addr,
        .log_base_2_of_page_size = log_base_2_of_page_size
    };
    #else
    const InterleavedAddrGen<beta_is_dram> addrb = {
        .bank_base_address = beta_addr,
        .page_size = page_size
    };
    #endif
    const uint32_t beta_tile_bytes = get_tile_size(cb_id_beta);
    #endif
    #ifdef FUSE_PRE_ADD
    const uint32_t src1_tile_bytes = get_tile_size(cb_id_in1);
    const DataFormat src1_data_format = get_dataformat(cb_id_in1);
    const InterleavedAddrGenFast<src1_is_dram> src_b = {
        .bank_base_address = b_addr,
        .page_size = src1_tile_bytes,
        .data_format = src1_data_format
    };
    #endif


    // Generate constant tiles for layernorm compute
    generate_bcast_scaler();
    generate_epsilon();

    // read a ublock of tiles from src to CB, and then push the ublock to unpacker
    uint32_t offs = 0;

    for (uint32_t ncht = 0; ncht < NCHt; ncht++) {
        for (uint32_t wt = 0; wt<Wt; wt += blk) {
            cb_reserve_back(cb_id_in0, blk);
            uint32_t l1_write_addr = get_write_ptr(cb_id_in0);

            for (uint32_t r = 0; r<blk; r++) {
                noc_async_read_tile(offs+wt+r+tile_offset, src_a, l1_write_addr);
                l1_write_addr += src0_tile_bytes;
            }
            noc_async_read_barrier();
            cb_push_back(cb_id_in0, blk);

            #ifdef FUSE_PRE_ADD
            // TODO(AP): refactor the ifdefs
            cb_reserve_back(cb_id_in1, blk);
            l1_write_addr = get_write_ptr(cb_id_in1);
            for (uint32_t r = 0; r<blk; r++) {
                noc_async_read_tile(offs+wt+r+tile_offset, src_b, l1_write_addr);
                l1_write_addr += src1_tile_bytes;
            }
            noc_async_read_barrier();
            cb_push_back(cb_id_in1, blk);
            #endif
        } // wt loop

        #if defined FUSE_GAMMA || defined FUSE_BETA
        if (ncht == 0) {
            for (uint32_t wt = 0; wt<Wt; wt += blk) {
                #ifdef FUSE_GAMMA
                {
                    cb_reserve_back(cb_id_gamma, blk);
                    uint32_t l1_write_addr = get_write_ptr(cb_id_gamma);
                    for (uint32_t r = 0; r<blk; r++) {
                        uint64_t gamma_noc_addr = get_noc_addr(wt + r, addrg);
                        noc_async_read(gamma_noc_addr, l1_write_addr, 32);
                        gamma_noc_addr += 32;
                        noc_async_read(gamma_noc_addr, l1_write_addr + 512, 32);
                        l1_write_addr += gamma_tile_bytes;
                    }
                    noc_async_read_barrier();
                    cb_push_back(cb_id_gamma, blk);
                }
                #endif

                #ifdef FUSE_BETA
                {
                    cb_reserve_back(cb_id_beta, blk);
                    uint32_t l1_write_addr = get_write_ptr(cb_id_beta);
                    for (uint32_t r = 0; r<blk; r++) {
                         uint64_t beta_noc_addr = get_noc_addr(wt + r, addrb);
                        noc_async_read(beta_noc_addr, l1_write_addr, 32);
                        beta_noc_addr += 32;
                        noc_async_read(beta_noc_addr, l1_write_addr + 512, 32);
                        l1_write_addr += beta_tile_bytes;
                    }
                    noc_async_read_barrier();
                    cb_push_back(cb_id_beta, blk);
                }
                #endif
            } // wt loop
        }
        #endif
        offs += Wt;
    } // ncht loop
}
