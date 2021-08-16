/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, Unikie 
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define ZF_LOG_LEVEL ZF_LOG_ERROR

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <utils/util.h>
#include <camkes/gdb/serial.h>
#include <camkes/gdb/gdb.h>

gdb_buffer_t buf;


static void send_message(char *message, int len);
static int handle_command(char *command, gdb_state_t *gdb_state);

static void GDB_write_register(char *command, gdb_state_t *gdb_state);
static void GDB_read_memory(char *command);
static void GDB_write_memory(char *command);
static void GDB_write_memory_binary(char *command);
static void GDB_query(char *command);
static void GDB_set_thread(char *command);
static void GDB_stop_reason(char *command, gdb_state_t *gdb_state);
static void GDB_read_general_registers(char *command, gdb_state_t *gdb_state);
static void GDB_read_register(char *command, gdb_state_t *gdb_state);
static void GDB_vcont(char *command, gdb_state_t *gdb_state);
static void GDB_continue(char *command, gdb_state_t *gdb_state);
static void GDB_step(char *command, gdb_state_t *gdb_state);
static void GDB_breakpoint(char *command, bool insert, gdb_state_t *gdb_state);


#define SEL4_REGISTER_IDX(reg)    ((size_t)OFFSETOF(seL4_UserContext, reg))
#define INVALID_SEL4_REGISTER_IDX ((size_t)-1)


// For the GDB register definitions in GDB sources,
// see 
// "/gdb/features/"
// "/gdb/features/arm/"
//
// For the seL4 register definitions in seL4 sources,
// see 
// "/seL4/libsel4/sel4_arch_include/$ARCH/sel4/sel4_arch/types.h"
// "/seL4/include/arch/$ARCH/arch/32/mode/machine/registerset.h"
// "/seL4/include/arch/$ARCH/arch/64/mode/machine/registerset.h"

#if defined(CONFIG_ARCH_IA32)

// GDB expected order of registers
typedef enum _x86_gdb_registers
{
    X86_GDB_REGISTER_eax     = 0,
    X86_GDB_REGISTER_ecx     = 1,
    X86_GDB_REGISTER_edx     = 2,
    X86_GDB_REGISTER_ebx     = 3,
    X86_GDB_REGISTER_esp     = 4,
    X86_GDB_REGISTER_ebp     = 5,
    X86_GDB_REGISTER_esi     = 6,
    X86_GDB_REGISTER_edi     = 7,
    X86_GDB_REGISTER_eip     = 8,
    X86_GDB_REGISTER_eflags  = 9,
    X86_GDB_REGISTER_cs      = 10,
    X86_GDB_REGISTER_ss      = 11,
    X86_GDB_REGISTER_ds      = 12,
    X86_GDB_REGISTER_es      = 13,
    X86_GDB_REGISTER_fs      = 14,
    X86_GDB_REGISTER_gs      = 15,
    
    X86_NUM_GDB_REGISTERS
} gdb_register_t;

#define NUM_GDB_REGISTERS (X86_NUM_GDB_REGISTERS)
    
static const size_t gdb_to_seL4_register_index[NUM_GDB_REGISTERS] = 
{
    [X86_GDB_REGISTER_eax]     = SEL4_REGISTER_IDX(eax),
    [X86_GDB_REGISTER_ecx]     = SEL4_REGISTER_IDX(ecx),
    [X86_GDB_REGISTER_edx]     = SEL4_REGISTER_IDX(edx),
    [X86_GDB_REGISTER_ebx]     = SEL4_REGISTER_IDX(ebx),
    [X86_GDB_REGISTER_esp]     = SEL4_REGISTER_IDX(esp),
    [X86_GDB_REGISTER_ebp]     = SEL4_REGISTER_IDX(ebp),
    [X86_GDB_REGISTER_esi]     = SEL4_REGISTER_IDX(esi),
    [X86_GDB_REGISTER_edi]     = SEL4_REGISTER_IDX(edi),
    [X86_GDB_REGISTER_eip]     = SEL4_REGISTER_IDX(eip),
    [X86_GDB_REGISTER_eflags]  = SEL4_REGISTER_IDX(eflags),
    [X86_GDB_REGISTER_cs]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_GDB_REGISTER_ss]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_GDB_REGISTER_ds]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_GDB_REGISTER_es]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_GDB_REGISTER_fs]      = SEL4_REGISTER_IDX(fs_base),
    [X86_GDB_REGISTER_gs]      = SEL4_REGISTER_IDX(gs_base)
};

#elif defined(CONFIG_ARCH_X86_64)

// GDB expected order of registers
typedef enum _x86_64_gdb_registers
{
    X86_64_GDB_REGISTER_rax     = 0,
    X86_64_GDB_REGISTER_rbx     = 1,
    X86_64_GDB_REGISTER_rcx     = 2,
    X86_64_GDB_REGISTER_rdx     = 3,
    X86_64_GDB_REGISTER_rsi     = 4,
    X86_64_GDB_REGISTER_rdi     = 5,
    X86_64_GDB_REGISTER_rbp     = 6,
    X86_64_GDB_REGISTER_rsp     = 7,
    X86_64_GDB_REGISTER_r8      = 8,
    X86_64_GDB_REGISTER_r9      = 9,
    X86_64_GDB_REGISTER_r10     = 10,
    X86_64_GDB_REGISTER_r11     = 11,
    X86_64_GDB_REGISTER_r12     = 12,
    X86_64_GDB_REGISTER_r13     = 13,
    X86_64_GDB_REGISTER_r14     = 14,
    X86_64_GDB_REGISTER_r15     = 15,
    X86_64_GDB_REGISTER_rip     = 16,
    X86_64_GDB_REGISTER_eflags  = 17,
    X86_64_GDB_REGISTER_cs      = 18,
    X86_64_GDB_REGISTER_ss      = 19,
    X86_64_GDB_REGISTER_ds      = 20,
    X86_64_GDB_REGISTER_es      = 21,
    X86_64_GDB_REGISTER_fs      = 22,
    X86_64_GDB_REGISTER_gs      = 23,

    X86_64_NUM_GDB_REGISTERS
} gdb_register_t;

#define NUM_GDB_REGISTERS (X86_64_NUM_GDB_REGISTERS)
    
static const size_t gdb_to_seL4_register_index[NUM_GDB_REGISTERS] = 
{
    [X86_64_GDB_REGISTER_rax]     = SEL4_REGISTER_IDX(rax),
    [X86_64_GDB_REGISTER_rbx]     = SEL4_REGISTER_IDX(rbx),
    [X86_64_GDB_REGISTER_rcx]     = SEL4_REGISTER_IDX(rcx),
    [X86_64_GDB_REGISTER_rdx]     = SEL4_REGISTER_IDX(rdx),
    [X86_64_GDB_REGISTER_rsi]     = SEL4_REGISTER_IDX(rsi),
    [X86_64_GDB_REGISTER_rdi]     = SEL4_REGISTER_IDX(rdi),
    [X86_64_GDB_REGISTER_rbp]     = SEL4_REGISTER_IDX(rbp),
    [X86_64_GDB_REGISTER_rsp]     = SEL4_REGISTER_IDX(rsp),
    [X86_64_GDB_REGISTER_r8]      = SEL4_REGISTER_IDX(r8),
    [X86_64_GDB_REGISTER_r9]      = SEL4_REGISTER_IDX(r9),
    [X86_64_GDB_REGISTER_r10]     = SEL4_REGISTER_IDX(r10),
    [X86_64_GDB_REGISTER_r11]     = SEL4_REGISTER_IDX(r11),
    [X86_64_GDB_REGISTER_r12]     = SEL4_REGISTER_IDX(r12),
    [X86_64_GDB_REGISTER_r13]     = SEL4_REGISTER_IDX(r13),
    [X86_64_GDB_REGISTER_r14]     = SEL4_REGISTER_IDX(r14),
    [X86_64_GDB_REGISTER_r15]     = SEL4_REGISTER_IDX(r15),
    [X86_64_GDB_REGISTER_rip]     = SEL4_REGISTER_IDX(rip),
    [X86_64_GDB_REGISTER_eflags]  = SEL4_REGISTER_IDX(eflags),
    [X86_64_GDB_REGISTER_cs]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_64_GDB_REGISTER_ss]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_64_GDB_REGISTER_ds]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_64_GDB_REGISTER_es]      = INVALID_SEL4_REGISTER_IDX, // Does not exist in seL4
    [X86_64_GDB_REGISTER_fs]      = SEL4_REGISTER_IDX(fs_base),
    [X86_64_GDB_REGISTER_gs]      = SEL4_REGISTER_IDX(gs_base)
};

#elif defined(CONFIG_ARCH_AARCH32)

// GDB expected order of registers
typedef enum _aarch32_gdb_registers
{
    AARCH32_GDB_REGISTER_r0    = 0,
    AARCH32_GDB_REGISTER_r1    = 1,
    AARCH32_GDB_REGISTER_r2    = 2,
    AARCH32_GDB_REGISTER_r3    = 3,
    AARCH32_GDB_REGISTER_r4    = 4,
    AARCH32_GDB_REGISTER_r5    = 5,
    AARCH32_GDB_REGISTER_r6    = 6,
    AARCH32_GDB_REGISTER_r7    = 7,
    AARCH32_GDB_REGISTER_r8    = 8,
    AARCH32_GDB_REGISTER_r9    = 9,
    AARCH32_GDB_REGISTER_r10   = 10,
    AARCH32_GDB_REGISTER_r11   = 11,
    AARCH32_GDB_REGISTER_r12   = 12,
    AARCH32_GDB_REGISTER_sp    = 13,
    AARCH32_GDB_REGISTER_lr    = 14, // LR == R14 in seL4
    AARCH32_GDB_REGISTER_pc    = 15,
    
    // From GDB arm-core.xml:
    //
    // "The CPSR is register 25, rather than register 16, because
    // the FPA registers historically were placed between the PC
    // and the CPSR in the "g" packet."
    
    AARCH32_GDB_REGISTER_cpsr  = 25,
    
    AARCH32_NUM_GDB_REGISTERS
} gdb_register_t;

#define NUM_GDB_REGISTERS (AARCH32_NUM_GDB_REGISTERS)
    
static const size_t gdb_to_seL4_register_index[NUM_GDB_REGISTERS] = 
{
    [AARCH32_GDB_REGISTER_r0]     = SEL4_REGISTER_IDX(r0),
    [AARCH32_GDB_REGISTER_r1]     = SEL4_REGISTER_IDX(r1),
    [AARCH32_GDB_REGISTER_r2]     = SEL4_REGISTER_IDX(r2),
    [AARCH32_GDB_REGISTER_r3]     = SEL4_REGISTER_IDX(r3),
    [AARCH32_GDB_REGISTER_r4]     = SEL4_REGISTER_IDX(r4),
    [AARCH32_GDB_REGISTER_r5]     = SEL4_REGISTER_IDX(r5),
    [AARCH32_GDB_REGISTER_r6]     = SEL4_REGISTER_IDX(r6),
    [AARCH32_GDB_REGISTER_r7]     = SEL4_REGISTER_IDX(r7),
    [AARCH32_GDB_REGISTER_r8]     = SEL4_REGISTER_IDX(r8),
    [AARCH32_GDB_REGISTER_r9]     = SEL4_REGISTER_IDX(r9),
    [AARCH32_GDB_REGISTER_r10]    = SEL4_REGISTER_IDX(r10),
    [AARCH32_GDB_REGISTER_r11]    = SEL4_REGISTER_IDX(r11),
    [AARCH32_GDB_REGISTER_r12]    = SEL4_REGISTER_IDX(r12),
    [AARCH32_GDB_REGISTER_sp]     = SEL4_REGISTER_IDX(sp),
    [AARCH32_GDB_REGISTER_lr]     = SEL4_REGISTER_IDX(r14), // LR == R14 in seL4
    [AARCH32_GDB_REGISTER_pc]     = SEL4_REGISTER_IDX(pc),
    [16]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [17]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [18]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [19]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [20]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [21]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [22]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [23]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [24]                          = INVALID_SEL4_REGISTER_IDX, // Does not exist
    [AARCH32_GDB_REGISTER_cpsr]   = SEL4_REGISTER_IDX(cpsr)
};

#elif defined(CONFIG_ARCH_AARCH64)

// GDB expected order of registers
typedef enum _aarch64_gdb_registers
{
    AARCH64_GDB_REGISTER_x0    = 0,
    AARCH64_GDB_REGISTER_x1    = 1,
    AARCH64_GDB_REGISTER_x2    = 2,
    AARCH64_GDB_REGISTER_x3    = 3,
    AARCH64_GDB_REGISTER_x4    = 4,
    AARCH64_GDB_REGISTER_x5    = 5,
    AARCH64_GDB_REGISTER_x6    = 6,
    AARCH64_GDB_REGISTER_x7    = 7,
    AARCH64_GDB_REGISTER_x8    = 8,
    AARCH64_GDB_REGISTER_x9    = 9,
    AARCH64_GDB_REGISTER_x10   = 10,
    AARCH64_GDB_REGISTER_x11   = 11,
    AARCH64_GDB_REGISTER_x12   = 12,
    AARCH64_GDB_REGISTER_x13   = 13,
    AARCH64_GDB_REGISTER_x14   = 14,
    AARCH64_GDB_REGISTER_x15   = 15,
    AARCH64_GDB_REGISTER_x16   = 16,
    AARCH64_GDB_REGISTER_x17   = 17,
    AARCH64_GDB_REGISTER_x18   = 18,
    AARCH64_GDB_REGISTER_x19   = 19,
    AARCH64_GDB_REGISTER_x20   = 20,
    AARCH64_GDB_REGISTER_x21   = 21,
    AARCH64_GDB_REGISTER_x22   = 22,
    AARCH64_GDB_REGISTER_x23   = 23,
    AARCH64_GDB_REGISTER_x24   = 24,
    AARCH64_GDB_REGISTER_x25   = 25,
    AARCH64_GDB_REGISTER_x26   = 26,
    AARCH64_GDB_REGISTER_x27   = 27,
    AARCH64_GDB_REGISTER_x28   = 28,
    AARCH64_GDB_REGISTER_x29   = 29,
    AARCH64_GDB_REGISTER_x30   = 30, // x30 or LR in seL4
    AARCH64_GDB_REGISTER_sp    = 31,
    AARCH64_GDB_REGISTER_pc    = 32,
    AARCH64_GDB_REGISTER_cpsr  = 33,
    
    AARCH64_NUM_GDB_REGISTERS
} gdb_register_t;

#define NUM_GDB_REGISTERS (AARCH64_NUM_GDB_REGISTERS)
    
static const size_t gdb_to_seL4_register_index[NUM_GDB_REGISTERS] = 
{
    [AARCH64_GDB_REGISTER_x0]    = SEL4_REGISTER_IDX(x0),
    [AARCH64_GDB_REGISTER_x1]    = SEL4_REGISTER_IDX(x1),   
    [AARCH64_GDB_REGISTER_x2]    = SEL4_REGISTER_IDX(x2),   
    [AARCH64_GDB_REGISTER_x3]    = SEL4_REGISTER_IDX(x3),   
    [AARCH64_GDB_REGISTER_x4]    = SEL4_REGISTER_IDX(x4),   
    [AARCH64_GDB_REGISTER_x5]    = SEL4_REGISTER_IDX(x5),   
    [AARCH64_GDB_REGISTER_x6]    = SEL4_REGISTER_IDX(x6),   
    [AARCH64_GDB_REGISTER_x7]    = SEL4_REGISTER_IDX(x7),   
    [AARCH64_GDB_REGISTER_x8]    = SEL4_REGISTER_IDX(x8),   
    [AARCH64_GDB_REGISTER_x9]    = SEL4_REGISTER_IDX(x9),   
    [AARCH64_GDB_REGISTER_x10]   = SEL4_REGISTER_IDX(x10),    
    [AARCH64_GDB_REGISTER_x11]   = SEL4_REGISTER_IDX(x11),    
    [AARCH64_GDB_REGISTER_x12]   = SEL4_REGISTER_IDX(x12),    
    [AARCH64_GDB_REGISTER_x13]   = SEL4_REGISTER_IDX(x13),    
    [AARCH64_GDB_REGISTER_x14]   = SEL4_REGISTER_IDX(x14),    
    [AARCH64_GDB_REGISTER_x15]   = SEL4_REGISTER_IDX(x15),    
    [AARCH64_GDB_REGISTER_x16]   = SEL4_REGISTER_IDX(x16),    
    [AARCH64_GDB_REGISTER_x17]   = SEL4_REGISTER_IDX(x17),    
    [AARCH64_GDB_REGISTER_x18]   = SEL4_REGISTER_IDX(x18),    
    [AARCH64_GDB_REGISTER_x19]   = SEL4_REGISTER_IDX(x19),    
    [AARCH64_GDB_REGISTER_x20]   = SEL4_REGISTER_IDX(x20),    
    [AARCH64_GDB_REGISTER_x21]   = SEL4_REGISTER_IDX(x21),    
    [AARCH64_GDB_REGISTER_x22]   = SEL4_REGISTER_IDX(x22),    
    [AARCH64_GDB_REGISTER_x23]   = SEL4_REGISTER_IDX(x23),    
    [AARCH64_GDB_REGISTER_x24]   = SEL4_REGISTER_IDX(x24),    
    [AARCH64_GDB_REGISTER_x25]   = SEL4_REGISTER_IDX(x25),    
    [AARCH64_GDB_REGISTER_x26]   = SEL4_REGISTER_IDX(x26),    
    [AARCH64_GDB_REGISTER_x27]   = SEL4_REGISTER_IDX(x27),    
    [AARCH64_GDB_REGISTER_x28]   = SEL4_REGISTER_IDX(x28),    
    [AARCH64_GDB_REGISTER_x29]   = SEL4_REGISTER_IDX(x29),    
    [AARCH64_GDB_REGISTER_x30]   = SEL4_REGISTER_IDX(x30), // x30 or LR in seL4
    [AARCH64_GDB_REGISTER_sp]    = SEL4_REGISTER_IDX(sp),    
    [AARCH64_GDB_REGISTER_pc]    = SEL4_REGISTER_IDX(pc),    
    [AARCH64_GDB_REGISTER_cpsr]  = SEL4_REGISTER_IDX(cpsr)    
};

#elif defined(CONFIG_ARCH_RISCV)
#error "RISCV not supported yet!"
#else
#error "Unknown architecture specified!"
#endif


/** Parse GDB register index to corresponding `seL4_UserContext` register structure index.
 *
 * @param gdb_register GDB register index.
 * @return `seL4_UserContext` register index or `INVALID_SEL4_REGISTER_IDX` if the corresponding register does not exist in `seL4_UserContext` structure.
 */
static inline size_t gdb_register_idx_to_seL4_UserContext_idx(gdb_register_t gdb_reg)
{
    size_t uc_idx = (size_t)INVALID_SEL4_REGISTER_IDX;
    
    if (gdb_reg < NUM_GDB_REGISTERS) {
        uc_idx = gdb_to_seL4_register_index[gdb_reg];
    }
}

static inline size_t x86_GDB_Register_to_seL4_UserContext(x86_gdb_registers gdb_register)
{

    size_t index = -1;
    if (gdb_register < x86_MAX_REGISTERS) {
        index = gdb_to_seL4_register_index[gdb_register];
    }
    if (index != -1) {
        /* Turn byte offset into index */
        index /= sizeof(seL4_Word);
    }
    return index;
}


// Compute a checksum for the GDB remote protocol
static unsigned char compute_checksum(char *data, int length)
{
    unsigned char checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum += (unsigned char) data[i];
    }
    return checksum;
}

static void string_to_word_data(char *string, seL4_Word *dest)
{
    char buf[sizeof(seL4_Word) * 2] = {0};
    strncpy(buf, string, sizeof(seL4_Word) * 2);
    *dest = (seL4_Word) strtoul((char *) buf, NULL, HEX_STRING);
}

static int get_breakpoint_format(gdb_BreakpointType type,
                                 seL4_Word *break_type, seL4_Word *rw)
{
    int err = 0;
    ZF_LOGD("Breakpoint type %d", type);
    switch (type) {
#ifdef CONFIG_HARDWARE_DEBUG_API
    case gdb_HardwareBreakpoint:
        *break_type = seL4_InstructionBreakpoint;
        *rw = seL4_BreakOnRead;
        err = 0;
        break;
    case gdb_WriteWatchpoint:
        *break_type = seL4_DataBreakpoint;
        *rw = seL4_BreakOnWrite;
        err = 0;
        break;
    case gdb_ReadWatchpoint:
        *break_type = seL4_DataBreakpoint;
        *rw = seL4_BreakOnRead;
        err = 0;
        break;
    case gdb_AccessWatchpoint:
        *break_type = seL4_DataBreakpoint;
        *rw = seL4_BreakOnReadWrite;
        err = 0;
        break;
#endif /* CONFIG_HARDWARE_DEBUG_API */
    default:
        // Unknown type
        err = 1;
    }
    return err;
}

int gdb_handle_fault(gdb_state_t *gdb_state)
{
    char watch_message[50];
    switch (gdb_state->stop_reason) {
    case stop_watch:
        ZF_LOGD("Hit watchpoint");
        snprintf(watch_message, 49, "T05thread:01;watch:%08x;", gdb_state->stop_watch_addr);
        send_message(watch_message, 0);
        break;
    case stop_hw_break:
        ZF_LOGD("Hit breakpoint");
        send_message("T05thread:01;hwbreak:;", 0);
        break;
    case stop_step:
        ZF_LOGD("Did step");
        send_message("T05thread:01;", 0);
        break;
    case stop_sw_break:
        ZF_LOGD("Software breakpoint");
        send_message("T05thread:01;swbreak:;", 0);
        break;
    case stop_none:
        ZF_LOGE("Unknown stop reason");
        send_message("T05thread:01;", 0);
        break;
    default:
        ZF_LOGF("Invalid stop reason.");

    }
    return 0;
}

int handle_gdb(gdb_state_t *gdb_state)
{
    // Get command and checksum
    int command_length = buf.checksum_index - 1;
    char *command_ptr = &buf.data[GDB_COMMAND_START_IDX];
    char command[GETCHAR_BUFSIZ + 1] = {0};
    strncpy(command, command_ptr, command_length);
    char *checksum = &buf.data[buf.checksum_index + 1];
    // Calculate checksum of data
    ZF_LOGD("command: %s", command);
    unsigned char computed_checksum = compute_checksum(command,
                                                       command_length);
    unsigned char received_checksum = (unsigned char) strtol(checksum,
                                                             NULL,
                                                             HEX_STRING);
    if (computed_checksum != received_checksum) {
        ZF_LOGD("Checksum error, computed %x,"
                "received %x received_checksum\n",
                computed_checksum, received_checksum);
        // Acknowledge packet
        gdb_printf(GDB_RESPONSE_START_STR GDB_NACK_STR GDB_RESPONSE_END_STR "\n");
    } else {
        // Acknowledge packet
        gdb_printf(GDB_RESPONSE_START_STR GDB_ACK_STR GDB_RESPONSE_END_STR "\n");
        // Parse the command
        handle_command(command, gdb_state);
    }

    return 0;
}


// Send a message with the GDB remote protocol
static void send_message(char *message, int len)
{
    int actual_len = strlen(message);
    if (len == 0) {
        len = actual_len + 1;
        ZF_LOGD("Setting length %p", __builtin_return_address(0));
    } else if ((actual_len + 1) != len) {
        ZF_LOGE("message length invalid: %s, %d, %d, %p", message, len, actual_len, __builtin_return_address(0));
    } else {
        ZF_LOGD("Correct length %p", __builtin_return_address(0));
    }
    ZF_LOGD("message: %s", message);
    unsigned char checksum = compute_checksum(message, len);
    gdb_printf(GDB_RESPONSE_START_STR "$%s#%02X\n", message, checksum);
    gdb_printf(GDB_RESPONSE_END_STR);
}


// GDB read memory command format:
// m[addr],[length]
static void GDB_read_memory(char *command)
{
    int err;
    char *token_ptr;
    // Get args from command
    char *addr_string = strtok_r(command, "m,", &token_ptr);
    char *length_string = strtok_r(NULL, ",", &token_ptr);
    // Convert strings to values
    seL4_Word addr = (seL4_Word) strtol(addr_string, NULL, HEX_STRING);
    seL4_Word length = (seL4_Word) strtol(length_string, NULL,
                                          DEC_STRING);
    if (length >= MAX_MEM_RANGE) {
        ZF_LOGE("Invalid read memory length %d", length);
        send_message("E01", 0);
        return;
    }

    if (addr == (seL4_Word) NULL) {
        ZF_LOGE("Bad memory address 0x%08x", addr);
        send_message("E01", 0);
        return;
    }
    // Buffer for raw data
    delegate_mem_range_t data;
    // Buffer for data formatted as hex string
    size_t buf_len = CHAR_HEX_SIZE * length + 1;
    char data_string[buf_len];
    memset(data_string, 0, buf_len);
    // Do a read call to the GDB delegate who will read from memory
    // on our behalf
    err = delegate_read_memory(addr, length, &data);

    if (err) {
        send_message("E01", 0);
    } else {
        // Format the data
        for (int i = 0; i < length; i++) {
            snprintf(&data_string[CHAR_HEX_SIZE * i], 3, "%02x", data.data[i] & 0xff);
        }
        send_message(data_string, buf_len);
    }
}

// GDB write memory command format:
// M[addr],[length]:[data]
static void GDB_write_memory(char *command)
{
    char *token_ptr;
    int err;
    // Get args from command
    char *addr_string = strtok_r(command, "M,", &token_ptr);
    char *length_string = strtok_r(NULL, ",:", &token_ptr);
    char *data_string = strtok_r(NULL, ":", &token_ptr);
    // Convert strings to values
    seL4_Word addr = (seL4_Word) strtol(addr_string, NULL, HEX_STRING);
    seL4_Word length = (seL4_Word) strtol(length_string, NULL, DEC_STRING);

    if (length >= MAX_MEM_RANGE) {
        ZF_LOGE("Invalid read memory length %d", length);
        send_message("E01", 0);
        return;
    }

    if (addr == (seL4_Word) NULL) {
        ZF_LOGE("Bad memory address 0x%08x", addr);
        send_message("E01", 0);
        return;
    }
    // Buffer for data to be written
    delegate_mem_range_t data;
    memset(data.data, 0, length);
    // Parse data to be written as raw hex
    for (int i = 0; i < length; i++) {
        sscanf(data_string, "%2hhx", &data.data[i]);
        data_string += CHAR_HEX_SIZE;
    }
    // Do a write call to the GDB delegate who will write to memory
    // on our behalf
    err = delegate_write_memory(addr, length, data);
    if (err) {
        send_message("E01", 0);
    } else {
        send_message("OK", 0);
    }
}

// GDB write binary memory command format:
// X[addr],[length]:[data]
static void GDB_write_memory_binary(char *command)
{
    char *token_ptr;
    // Get args from command
    char *addr_string = strtok_r(command, "X,", &token_ptr);
    char *length_string = strtok_r(NULL, ",:", &token_ptr);
    // Convert strings to values
    seL4_Word addr = strtol(addr_string, NULL, HEX_STRING);
    seL4_Word length = strtol(length_string, NULL, DEC_STRING);
    delegate_mem_range_t data = {0};
    if (length == 0) {
        ZF_LOGW("Writing 0 length");
        send_message("OK", 0);
        return;
    }

    void *bin_data = strtok_r(NULL, ":", &token_ptr);
    // Copy the raw data to the expected location
    if (bin_data == NULL) {
        ZF_LOGE("data is NULL");
        send_message("E01", 0);
        return;
    }
    memcpy(&data.data, bin_data, length);

    // Do a write call to the GDB delegate who will write to memory
    // on our behalf
    int err = delegate_write_memory(addr, length, data);
    if (err) {
        send_message("E01", 0);
    } else {
        send_message("OK", 0);
    }
}

// GDB query command format:
// q[query]...
static void GDB_query(char *command)
{
    char *token_ptr;
    ZF_LOGD("query: %s", command);
    char *query_type = strtok_r(command, "q:", &token_ptr);
    if (strcmp("Supported", query_type) == 0) {// Setup argument storage
        send_message("swbreak+;hwbreak+;PacketSize=100", 0);
        // Most of these query messages can be ignored for basic functionality
    } else if (!strcmp("TStatus", query_type)) {
        send_message("", 0);
    } else if (!strcmp("TfV", query_type)) {
        send_message("", 0);
    } else if (!strcmp("C", query_type)) {
        send_message("QC1", 0);
    } else if (!strcmp("Attached", query_type)) {
        send_message("", 0);
    } else if (!strcmp("fThreadInfo", query_type)) {
        send_message("m01", 0);
    } else if (!strcmp("sThreadInfo", query_type)) {
        send_message("l", 0);
    } else if (!strcmp("Symbol", query_type)) {
        send_message("", 0);
    } else if (!strcmp("Offsets", query_type)) {
        send_message("", 0);
    } else {
        ZF_LOGD("Unrecognised query command");
        send_message("E01", 0);
    }
}

// Currently ignored
static void GDB_set_thread(char *command)
{
    send_message("OK", 0);
}

// Respond with the reason the thread being debuged stopped
static void GDB_stop_reason(char *command, gdb_state_t *gdb_state)
{
    switch (gdb_state->stop_reason) {
    case stop_hw_break:
        send_message("T05thread:01;hwbreak:;", 0);
        break;
    case stop_sw_break:
        send_message("T05thread:01;swbreak:;", 0);
        break;
    default:
        send_message("T05thread:01;", 0);
    }
}

static void GDB_read_general_registers(char *command, gdb_state_t *gdb_state)
{
    // seL4_Word registers[x86_MAX_REGISTERS] = {0};
    seL4_UserContext registers = {0};
    delegate_read_registers(gdb_state->current_thread_tcb, &registers);
    int buf_len = x86_MAX_REGISTERS * sizeof(seL4_Word) * CHAR_HEX_SIZE + 1;
    char data[buf_len];
    memset(data, 0, buf_len);
    // Read the register data from the buffer and marshall into a string
    // to send back to GDB, making sure the byte order is correct
    for (int i = 0; i < x86_MAX_REGISTERS; i++) {
        seL4_Word seL4_reg_num = x86_GDB_Register_to_seL4_UserContext(i);
        seL4_Word value;
        if (seL4_reg_num == -1) {
            ZF_LOGW("Invalid register number");
            value = 0;
        } else {
            value = ((seL4_Word *)&registers)[seL4_reg_num];
        }
        sprintf(data + sizeof(seL4_Word) * CHAR_HEX_SIZE * i,
                "%0*x", seL4_WordBits / 4, BSWAP_WORD(value));
    }
    send_message(data, buf_len);
}

// GDB read register command format:
// p[reg_num]
static void GDB_read_register(char *command, gdb_state_t *gdb_state)
{
    seL4_Word reg;
    char *token_ptr;
    // Get which register we want to read
    char *reg_string = strtok_r(&command[GDB_COMMAND_START_IDX], "", &token_ptr);
    if (reg_string == NULL) {
        send_message("E00", 0);
        return;
    }
    seL4_Word reg_num = strtol(reg_string, NULL, HEX_STRING);
    if (reg_num >= x86_VALID_REGISTERS) {
        send_message("E00", 0);
        return;
    }
    // Convert to the register order we have
    seL4_Word seL4_reg_num = x86_GDB_Register_to_seL4_UserContext(reg_num);
    if (seL4_reg_num == -1) {
        ZF_LOGE("Invalid GDB register number: %d", reg_num);
        send_message("E00", 0);
        return;
    } else {
        delegate_read_register(gdb_state->current_thread_tcb, &reg, seL4_reg_num);
    }
    int buf_len = sizeof(seL4_Word) * CHAR_HEX_SIZE + 1;
    char data[buf_len];
    data[buf_len - 1] = 0;
    // Send the register contents as a string, making sure
    // the byte order is correct
    sprintf(data, "%0*x", seL4_WordBits / 4, BSWAP_WORD(reg));
    send_message(data, buf_len);
}

static void GDB_write_general_registers(char *command, gdb_state_t *gdb_state)
{
    char *token_ptr;
    // Get args from command
    char *data_string = strtok_r(&command[GDB_COMMAND_START_IDX], "", &token_ptr);
    // Truncate data to register length
    int num_regs = sizeof(seL4_UserContext) / sizeof(seL4_Word);
    int num_regs_data = (strlen(data_string)) / (sizeof(seL4_Word) * 2);
    if (num_regs_data > num_regs) {
        num_regs_data = num_regs;
    }
    // Marshall data
    seL4_UserContext data;
    for (int i = 0; i < num_regs_data; i++) {
        seL4_Word seL4_register_number = x86_GDB_Register_to_seL4_UserContext(i);
        string_to_word_data(&data_string[2 * i * sizeof(seL4_Word)], ((seL4_Word *)&data) + seL4_register_number);
        ((seL4_Word *)&data)[seL4_register_number] = BSWAP_WORD(((seL4_Word *)&data)[seL4_register_number]);
    }
    delegate_write_registers(gdb_state->current_thread_tcb, data, num_regs_data);
    gdb_state->current_pc = data.eip;
    send_message("OK", 0);
}

// GDB write register command format:
// P[reg_num]=[data]
static void GDB_write_register(char *command, gdb_state_t *gdb_state)
{
    char *token_ptr;
    // Parse arguments
    char *reg_string = strtok_r(&command[GDB_COMMAND_START_IDX], "=", &token_ptr);
    char *data_string = strtok_r(NULL, "", &token_ptr);
    // If valid register, do something, otherwise reply OK
    seL4_Word reg_num = strtol(reg_string, NULL, HEX_STRING);
    if (reg_num < x86_GDB_REGISTERS) {
        // Convert arguments
        seL4_Word data;
        string_to_word_data(data_string, &data);
        data = BSWAP_WORD(data);
        // Convert to register order we have
        seL4_Word seL4_reg_num = x86_GDB_Register_to_seL4_UserContext(reg_num);
        if (seL4_reg_num == -1) {
            ZF_LOGE("Invalid GDB register number: %d, ignoring write", reg_num);
        } else {
            delegate_write_register(gdb_state->current_thread_tcb, data, seL4_reg_num);
            if (reg_num == GDBRegister_eip) {
                gdb_state->current_pc = data;
            }
        }
    }
    send_message("OK", 0);
}

static void GDB_vcont(char *command, gdb_state_t *gdb_state)
{
    if (!strncmp(&command[7], "c", 1)) {
        GDB_continue(command, gdb_state);
    } else if (!strncmp(&command[7], "s", 1)) {
        GDB_step(command, gdb_state);
    } else {
        send_message("", 0);
    }
}

static void GDB_continue(char *command, gdb_state_t *gdb_state)
{
    int err = 0;
    // If it's not a step exception, then we can resume
    // Otherwise, just resume by responding on the step fault
    if (gdb_state->current_thread_step_mode && gdb_state->stop_reason != stop_step) {
        err = delegate_resume(gdb_state->current_thread_tcb);
    }
    gdb_state->current_thread_step_mode = false;
    if (err) {
        ZF_LOGE("delegate resume failed\n");
        send_message("E01", 0);
    }

    gdb_state->sem_post();
}

static void GDB_step(char *command, gdb_state_t *gdb_state)
{
    int err = 0;
    // If it's not a step exception, then we need to set stepping
    // Otherwise, just step by responding on the step fault
    if (!gdb_state->current_thread_step_mode && gdb_state->stop_reason != stop_step) {
        ZF_LOGD("Entering step mode");
        err = delegate_step(gdb_state->current_thread_tcb);
    } else {
        ZF_LOGD("Already in step mode");
    }
    gdb_state->current_thread_step_mode = true;
    if (err) {
        ZF_LOGE("delegate step failed\n");
        send_message("E01", 0);
    }
    gdb_state->sem_post();
}

// GDB insert breakpoint command format:
// Z[type],[addr],[size]
static void GDB_breakpoint(char *command, bool insert, gdb_state_t *gdb_state)
{
    char *token_ptr;
    seL4_Word break_type;
    seL4_Word rw;
    // Parse arguments
    char *type_string = strtok_r(&command[GDB_COMMAND_START_IDX], ",", &token_ptr);
    char *addr_string = strtok_r(NULL, ",", &token_ptr);
    char *size_string = strtok_r(NULL, ",", &token_ptr);
    // Convert strings to values
    seL4_Word type = (seL4_Word) strtol(type_string, NULL, HEX_STRING);
    seL4_Word addr = (seL4_Word) strtol(addr_string, NULL, HEX_STRING);
    seL4_Word size = (seL4_Word) strtol(size_string, NULL, HEX_STRING);
    ZF_LOGD("Breakpoint: %s, type: %d, addr: 0x%x, size %d", insert ? "'insert'" : "'remove'", type, addr, size);
    // If this is a software breakpoint, then we will ignore
    // By ignoring this command, GDB will just use the read and write
    // memory commands to set a breakpoint itself. This can later be changed
    // if setting software breakpoints becomes supported by the kernel.
    if (type == gdb_SoftwareBreakpoint) {
        send_message("", 0);
    } else {
        int err;
        err = get_breakpoint_format(type, &break_type, &rw);
        if (!err) {
            // Hardware breakpoints can only be size 0
            if (type == gdb_HardwareBreakpoint) {
                size = 0;
            }
            if (insert) {
                err = delegate_insert_break(gdb_state->current_thread_tcb, break_type,
                                            addr, size,
                                            rw);
            } else {
                err = delegate_remove_break(gdb_state->current_thread_tcb, break_type,
                                            addr, size,
                                            rw);
            }
        }
        if (err) {
            ZF_LOGE("Couldn't set breakpoint");
            send_message("E01", 0);
        } else {
            send_message("OK", 0);
        }
    }
}


static int handle_command(char *command, gdb_state_t *gdb_state)
{
    switch (command[0]) {
    case '!':
        // Enable extended mode
        ZF_LOGE("Not implemented: enable extended mode");
        break;
    case '?':
        // Halt reason
        GDB_stop_reason(command, gdb_state);
        break;
    case 'A':
        // Argv
        ZF_LOGE("Not implemented: argv");
        break;
    case 'b':
        if (command[1] == 'c') {
            // Backward continue
            ZF_LOGE("Not implemented: backward continue");
            break;
        } else if (command[1] == 's') {
            // Backward step
            ZF_LOGE("Not implemented: backward step");
            break;
        } else {
            // Set baud rate
            ZF_LOGE("Not implemented: Set baud rate");
            break;
        }
    case 'c':
        // Continue
        ZF_LOGD("Continuing");
        GDB_continue(command, gdb_state);
        break;
    case 'C':
        // Continue with signal
        ZF_LOGE("Not implemented: continue with signal");
        break;
    case 'd':
        ZF_LOGE("Not implemented: toggle debug");
        break;
    case 'D':
        ZF_LOGE("Not implemented: detach");
        break;
    case 'F':
        ZF_LOGE("Not implemented: file IO");
        break;
    case 'g':
        ZF_LOGD("Reading general registers");
        GDB_read_general_registers(command, gdb_state);
        break;
    case 'G':
        ZF_LOGD("Write general registers");
        GDB_write_general_registers(command, gdb_state);
        break;
    case 'H':
        ZF_LOGD("Set thread ignored");
        GDB_set_thread(command);
        break;
    case 'i':
        ZF_LOGE("Not implemented: cycle step");
        break;
    case 'I':
        ZF_LOGE("Not implemented: signal + cycle step");
        break;
    case 'k':
        ZF_LOGE("Kill called.  Program will not finish");
        break;
    case 'm':
        ZF_LOGD("Reading memory");
        GDB_read_memory(command);
        break;
    case 'M':
        ZF_LOGD("Writing memory");
        GDB_write_memory(command);
        break;
    case 'p':
        ZF_LOGD("Read register");
        GDB_read_register(command, gdb_state);
        break;
    case 'P':
        ZF_LOGD("Write register");
        GDB_write_register(command, gdb_state);
        break;
    case 'q':
        ZF_LOGD("Query");
        GDB_query(command);
        break;
    case 'Q':
        ZF_LOGE("Not implemented: set");
        break;
    case 'r':
        ZF_LOGE("Not implemented: reset");
        break;
    case 'R':
        ZF_LOGE("Not implemented: restart");
        break;
    case 's':
        ZF_LOGD("Stepping");
        GDB_step(command, gdb_state);
        break;
    case 'S':
        ZF_LOGE("Not implemented: step + signal");
        break;
    case 't':
        ZF_LOGE("Not implemented: search");
        break;
    case 'T':
        ZF_LOGE("Not implemented: check thread");
        break;
    case 'v':
        if (!strncmp(&command[GDB_COMMAND_START_IDX], "Cont?", 5)) {
            send_message("vCont;c;s", 0);
        } else if (!strncmp(&command[GDB_COMMAND_START_IDX], "Cont", 4)) {
            GDB_vcont(command, gdb_state);
        } else if (!strncmp(&command[GDB_COMMAND_START_IDX], "Kill", 4)) {
            send_message("", 0);
        } else if (!strncmp(&command[GDB_COMMAND_START_IDX], "MustReplyEmpty", 14)) {
            send_message("", 0);
        } else {
            ZF_LOGE("Command not supported");
        }
        break;
    case 'X':
        ZF_LOGD("Writing memory, binary");
        GDB_write_memory_binary(command);
        break;
    case 'z':
        ZF_LOGD("Removing breakpoint");
        GDB_breakpoint(command, false, gdb_state);
        break;
    case 'Z':
        ZF_LOGD("Inserting breakpoint");
        GDB_breakpoint(command, true, gdb_state);
        break;
    default:
        ZF_LOGE("Unknown command");
    }

    return 0;
}

