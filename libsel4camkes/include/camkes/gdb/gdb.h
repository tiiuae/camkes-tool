/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, Unikie 
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4/sel4.h>
#include <machine/registerset.h>
#include <stdint.h>
#include <stdbool.h>
#include <camkes/gdb/delegate_types.h>


#define GDB_COMMAND_START_IDX         (1)  // GDB commands start from 2nd char
#define HEX_STRING_BASE               (16)
#define DEC_STRING_BASE               (10)
#define CHAR_HEX_SIZE                 (2)  // # of chars to represent 1 hex byte

// Colour coded response start/end
//#define GDB_RESPONSE_START_STR      "\x1b[31m"
//#define GDB_RESPONSE_END_STR        "\x1b[0m"

// Normal response start/end
#define GDB_RESPONSE_START_STR        ""
#define GDB_RESPONSE_END_STR          ""

#define GDB_ACK_STR                   "+"
#define GDB_NACK_STR                  "-"


typedef enum {
    stop_none,
    stop_sw_break,
    stop_hw_break,
    stop_step,
    stop_watch
} stop_reason_t;

typedef enum {
    GDB_SoftwareBreakpoint,
    GDB_HardwareBreakpoint,
    GDB_WriteWatchpoint,
    GDB_ReadWatchpoint,
    GDB_AccessWatchpoint
} gdb_breakpoint_t;

#define GETCHAR_BUFSIZ 512

typedef struct gdb_buffer {
    uint32_t length;
    uint32_t checksum_count;
    uint32_t checksum_index;
    char data[GETCHAR_BUFSIZ];
} gdb_buffer_t;

extern gdb_buffer_t buf;

typedef struct {
    /* Cap of currently selected thread in components cspace */
    seL4_Word current_thread_tcb;
    /* Current pc of the currently selected thread */
    seL4_Word current_pc;
    /* current thread's hw debugging step mode */
    bool current_thread_step_mode;
    /* Fault reason for the currently selected thread */
    stop_reason_t stop_reason;
    /* If fault was watch fault, then this is the address */
    seL4_Word stop_watch_addr;
    /* Callback function to wake thread's fault handler */
    int (*sem_post)(void);
} gdb_state_t;

int delegate_write_memory(seL4_Word addr, seL4_Word length, delegate_mem_range_t data);
int delegate_read_memory(seL4_Word addr, seL4_Word length, delegate_mem_range_t *data);
void delegate_read_registers(seL4_Word tcb_cap, seL4_UserContext *registers);
void delegate_read_register(seL4_Word tcb_cap, seL4_Word *reg, seL4_Word reg_num);
int delegate_write_registers(seL4_Word tcb_cap, seL4_UserContext registers, int len);
int delegate_write_register(seL4_Word tcb_cap, seL4_Word data, seL4_Word reg_num);
int delegate_insert_break(seL4_Word tcb_cap, seL4_Word type, seL4_Word addr, seL4_Word size, seL4_Word rw);
int delegate_remove_break(seL4_Word tcb_cap, seL4_Word type, seL4_Word addr, seL4_Word size, seL4_Word rw);
int delegate_resume(seL4_Word tcb_cap);
int delegate_step(seL4_Word tcb_cap);



int gdb_handle(gdb_state_t *gdb_state);
int gdb_handle_fault(gdb_state_t *gdb_state);
