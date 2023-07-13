#pragma once

// This file contains the memory map for the tensix device
//
// It is included on the device, on the host and in linker scripts to
// serve as a single source of truth for memory layout for both hw design and
// sw convention.  The requirement of including this in linker scripts
// requires that everything within be handled by the C pre-processor, hence
// the use of #define
//
// Before adding a define here, read the following:
// 1) Any "truly global" address must be specified explicitly here.  Truly
// global addresses are addresses that are referenced on both the host and
// device
// 2) Memory section sizes must be specified here, these are used in the
// linker scripts
// 3) Device static/global variables generally should NOT be listed here.  If
// they are global to a core, declare them in the that core's source code and
// tag them if needed with a section (e.g., "l1_data")
//

/////////////
// RISC-V Address map definition (hardware)
#define MEM_L1_BASE           0x0
#define MEM_L1_SIZE           (1464 * 1024)

#define MEM_LOCAL_BASE        0xFFB00000
#define MEM_BRISC_LOCAL_SIZE  (4 * 1024)
#define MEM_NCRISC_LOCAL_SIZE (4 * 1024)
#define MEM_TRISC_LOCAL_SIZE  (2 * 1024)

#define MEM_NCRISC_IRAM_BASE  0xFFC00000
#define MEM_NCRISC_IRAM_SIZE  (16 * 1024)

/////////////
// Firmware/kernel code holes
#define MEM_BOOT_CODE_SIZE             4
#define MEM_BRISC_FIRMWARE_SIZE        (8 * 1024)
#define MEM_NCRISC_FIRMWARE_SIZE       (16 * 1024)
#define MEM_TRISC0_SIZE                (16 * 1024)
#define MEM_TRISC1_SIZE                (12 * 1024)
#define MEM_TRISC2_SIZE                (16 * 1024)
#define MEM_ZEROS_SIZE                 512

#define MEM_BOOT_CODE_BASE             0
#define MEM_MAILBOX_BASE               4
#define MEM_ZEROS_BASE                 2048
#define MEM_BRISC_FIRMWARE_BASE        (MEM_ZEROS_BASE + MEM_ZEROS_SIZE)
#define MEM_NCRISC_FIRMWARE_BASE       MEM_NCRISC_IRAM_BASE
#define MEM_TRISC0_BASE                (MEM_BRISC_FIRMWARE_BASE + MEM_BRISC_FIRMWARE_SIZE)
#define MEM_TRISC1_BASE                (MEM_TRISC0_BASE + MEM_TRISC0_SIZE)
#define MEM_TRISC2_BASE                (MEM_TRISC1_BASE + MEM_TRISC1_SIZE)

/////////////
// Mailboxes
#define MEM_MAILBOX_BRISC_OFFSET       0
#define MEM_MAILBOX_TRISC0_OFFSET      4
#define MEM_MAILBOX_TRISC1_OFFSET      8
#define MEM_MAILBOX_TRISC2_OFFSET      12
#define MEM_MAILBOX_NCRISC_OFFSET      16
#define MEM_TEST_MAILBOX_ADDRESS       (MEM_MAILBOX_BASE +   4) // 4 bytes * 5 cores
#define MEM_FWLOG_MAILBOX_ADDRESS      (MEM_MAILBOX_BASE +  24) // 4 * 5 bytes * 5 cores
#define MEM_FWEVENT_MAILBOX_ADDRESS    (MEM_MAILBOX_BASE + 124) // 4 bytes * 5 cores
#define MEM_WALL_CLOCK_MAILBOX_ADDRESS (MEM_MAILBOX_BASE + 144) // 4 bytes * 1 core (brisc)
#define MEM_DEBUG_MAILBOX_ADDRESS      (MEM_MAILBOX_BASE + 148) // 16 bytes * 4 cores (not brisc)
#define MEM_DEBUG_MAILBOX_SIZE         64
#define MEM_MAILBOX_END                (MEM_MAILBOX_BASE + 212)

/////////////
// Initialization relocation L1 memory
// Host downloads to these addresses, fw copies to destination
// Note: using xmov to copy ncrisc to addresses above 1M hangs the chip
#define MEM_BRISC_INIT_LOCAL_L1_BASE      (MEM_TRISC2_BASE + MEM_TRISC2_SIZE)
#define MEM_NCRISC_INIT_LOCAL_L1_BASE     (MEM_BRISC_INIT_LOCAL_L1_BASE + MEM_BRISC_LOCAL_SIZE)
#define MEM_TRISC0_INIT_LOCAL_L1_BASE     (MEM_NCRISC_INIT_LOCAL_L1_BASE + MEM_NCRISC_LOCAL_SIZE)
#define MEM_TRISC1_INIT_LOCAL_L1_BASE     (MEM_TRISC0_INIT_LOCAL_L1_BASE + MEM_TRISC_LOCAL_SIZE)
#define MEM_TRISC2_INIT_LOCAL_L1_BASE     (MEM_TRISC1_INIT_LOCAL_L1_BASE + MEM_TRISC_LOCAL_SIZE)

#define MEM_NCRISC_INIT_IRAM_L1_BASE      (MEM_TRISC2_INIT_LOCAL_L1_BASE + MEM_TRISC_LOCAL_SIZE)

/////////////
// Stack info
// Increasing the stack size comes at the expense of less local memory for globals
#define MEM_BRISC_STACK_SIZE  1024
#define MEM_NCRISC_STACK_SIZE 1024
#define MEM_TRISC0_STACK_SIZE  256
#define MEM_TRISC1_STACK_SIZE  256
#define MEM_TRISC2_STACK_SIZE  768

#define MEM_BRISC_STACK_BASE  (MEM_LOCAL_BASE + MEM_BRISC_LOCAL_SIZE - MEM_BRISC_STACK_SIZE)
#define MEM_NCRISC_STACK_BASE (MEM_LOCAL_BASE + MEM_NCRISC_LOCAL_SIZE - MEM_NCRISC_STACK_SIZE)
#define MEM_TRISC0_STACK_BASE (MEM_LOCAL_BASE + MEM_TRISC_LOCAL_SIZE - MEM_TRISC0_STACK_SIZE)
#define MEM_TRISC1_STACK_BASE (MEM_LOCAL_BASE + MEM_TRISC_LOCAL_SIZE - MEM_TRISC1_STACK_SIZE)
#define MEM_TRISC2_STACK_BASE (MEM_LOCAL_BASE + MEM_TRISC_LOCAL_SIZE - MEM_TRISC2_STACK_SIZE)
