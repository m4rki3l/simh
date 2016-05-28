/* XXX dmd5620_defs.h: AT&T 3B2 Model 400 system-specific logic implementation

   Copyright (c) 2015, XXX

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.
*/

/*
    TODO (terminal)

    - host comms (via tmxr)
    - mouse
    - video: move daddr from sysdev.c
    - nvram: make persistent
    - rom: obtain unmodified 1.1 and 2.0 roms
    - document clocks, crystals, and video parameters
    - document memory map
    - document interrupts

    TODO (cpu core)
    - watchpoints
*/

#include "dmd5620_sys.h"

#include "3b2_cpu.h"
#include "dmd5620_sysdev.h"
#include "dmd5620_uart.h"
#include "3b2_mmu.h"

char sim_name[] = "AT&T Dot Mapped Display 5620";

REG *sim_PC = &cpu_reg[0];

/* All opcodes are 1 or 2 bytes. Operands may be up to 6 bytes, and
   there may be up to 3 operands, for a maximum of 20 bytes */
int32 sim_emax = 20;

extern instr *last_instruction;

DEVICE *sim_devices[] = {
    &cpu_dev,
    &mmu_dev,
    &uart_dev,
    &nvram_dev,
    &daddr_dev,
    &vc_dev,
    &mouse_dev,
    NULL
};

const char *sim_stop_messages[] = {
    "Unknown error",
    "Reserved Instruction",
    "Breakpoint",
    "Invalid Opcode",
    "IRQ",
    "Exception/Trap"
};

t_stat sim_load(FILE *fileref, char *cptr, char *fnam, int flag)
{
    int32 i;
    int32 addr = 0;
    int32 cnt = 0;

    if ((*cptr != 0) || (flag != 0)) {
        return SCPE_ARG;
    }

    addr = R[NUM_PC];

    while ((i = getc (fileref)) != EOF) {
        pwrite_b(addr, i);
        addr++;
        cnt++;
    }

    printf ("%d Bytes loaded.\n", cnt);

    return SCPE_OK;
}

t_stat parse_sym(char *cptr, t_addr exta, UNIT *uptr, t_value *val, int32 sw)
{
    DEVICE *dptr;
    uint32 addr = (uint32) exta;
    t_stat r;
    int32 k, num, vp;
    int32 len = 4;

    if (sw & SWMASK ('B')) {
        len = 1;
    } else if (sw & SWMASK ('H')) {
        len = 2;
    } else if (sw & SWMASK ('W')) {
        len = 4;
    }

    // Parse cptr
    num = (int32) get_uint(cptr, 16, WORD_MASK, &r);

    if (r != SCPE_OK) {
        return r;
    }

    if (uptr == NULL) {
        uptr = &cpu_unit;
    }

    dptr = find_dev_from_unit(uptr);

    if (dptr == NULL) {
        return SCPE_IERR;
    }

    vp = 0;
    for (k = len - 1; k >= 0; k--) {
        val[vp++] = (num >> (k * 8)) & 0xff;
    }

    return -(vp - 1);
}

t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw)
{
    uint32 len = 4;
    int32 k, vp, num;
    instr *ip;

    if (sw & SWMASK ('B')) {
        len = 1;
    } else if (sw & SWMASK ('H')) {
        len = 2;
    } else if (sw & SWMASK ('W')) {
        len = 4;
    }

    if (sw & SWMASK('M')) {
        if (last_instruction == NULL) {
            return 0;
        }

        fprint_sym_m(of, last_instruction);
        return 0;
    }

    num = 0;
    vp = 0;
    for (k = len - 1; k >= 0; k--) {
        num = num | (((int32) val[vp++]) << (k * 8));
    }

    fprint_val(of, (uint32) num, 16, len * 8, PV_RZRO);

    return -(vp - 1);
}