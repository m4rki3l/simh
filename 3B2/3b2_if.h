/* 3b2_cpu.h: AT&T 3B2 Model 400 Floppy (TMS2797NL) Header

   Copyright (c) 2014, Seth J. Morabito

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

   Except as contained in this notice, the name of Charles E. Owen shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Charles E. Owen.
*/

#ifndef __3B2_IF_H__
#define __3B2_IF_H__

#include "3b2_defs.h"
#include "3b2_sysdev.h"
#include "3b2_sys.h"

typedef struct {
    uint8 data;
    uint8 cmd;
    uint8 cmd_type;
    uint8 status;
    uint8 track;
    uint8 sector;
    uint8 side;

    t_bool drq;
} IF_STATE;


extern DEVICE if_dev;
extern DEBTAB sys_deb_tab[];
extern IF_STATE if_state;

#define IFBASE 0x4d000
#define IFSIZE 0x10

#define IF_STATUS_REG    0
#define IF_CMD_REG       0
#define IF_TRACK_REG     1
#define IF_SECTOR_REG    2
#define IF_DATA_REG      3

/* Status Bits */
#define IF_BUSY          0x01
#define IF_DRQ           0x02
#define IF_INDEX         0x02
#define IF_TK_0          0x04
#define IF_LOST_DATA     0x04
#define IF_CRC_ERR       0x08
#define IF_SEEK_ERR      0x10
#define IF_RNF           0x10
#define IF_HEAD_LOADED   0x20
#define IF_RECORD_TYPE   0x20
#define IF_WP            0x40
#define IF_NRDY          0x80

/* Type I Commands */
#define IF_RESTORE       0x00
#define IF_SEEK          0x10
#define IF_STEP          0x20
#define IF_STEP_T        0x30
#define IF_STEP_IN       0x40
#define IF_STEP_IN_T     0x50
#define IF_STEP_OUT      0x60
#define IF_STEP_OUT_T    0x70

/* Type II Commands */
#define IF_READ_SEC      0x80
#define IF_READ_SEC_M    0x90
#define IF_WRITE_SEC     0xA0
#define IF_WRITE_SEC_M   0xB0

/* Type III Commands */
#define IF_READ_ADDR     0xC0
#define IF_READ_TRACK    0xE0
#define IF_WRITE_TRACK   0xF0

/* Type IV Command */
#define IF_FORCE_INT     0xD0

/* Constants */

#define IF_SIDES         2
#define IF_TRACK_SIZE    4608
#define IF_SECTOR_SIZE   512
#define IF_TRACK_COUNT   80

#define IF_DSK_SIZE      (IF_SIDES * IF_TRACK_SIZE * IF_TRACK_COUNT)

/* Function prototypes */

t_stat if_svc(UNIT *uptr);
t_stat if_reset(DEVICE *dptr);
t_stat if_boot(int32 unitno, DEVICE *dptr);
uint32 if_read(uint32 pa, uint8 size);
void if_write(uint32 pa, uint32 val, uint8 size);
void if_drq_handled();

#endif
