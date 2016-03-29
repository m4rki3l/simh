/* 3b2_uart.c:  SCN2681A Dual UART Implementation

   Copyright (c) 2015, Seth J. Morabito

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

#include "3b2_uart.h"

/*
 * Registers
 */

/* The UART state */
struct uart_state u;

extern uint16 csr_data;

UNIT uart_unit = { UDATA(&uart_svc, TT_MODE_7B, 0), CLK_DELAY };

REG uart_reg[] = {
    { HRDATAD(ISTAT,  u.istat,          8, "Interrupt Status")            },
    { HRDATAD(IMASK,  u.imask,          8, "Interrupt Mask")              },
    { HRDATAD(ACR,    u.acr,            8, "Auxiliary Control Register")  },
    { HRDATAD(CTR,    u.c_set,         16, "Counter Setting")             },
    { HRDATAD(CTRV,   u.c_val,         16, "Counter Value")               },
    { HRDATAD(STAT_A, u.port[0].stat,   8, "Status  (Port A)")            },
    { HRDATAD(CMD_A,  u.port[0].cmd,    8, "Command (Port A)")            },
    { HRDATAD(DATA_A, u.port[0].buf,    8, "Data    (Port A)")            },
    { HRDATAD(STAT_B, u.port[1].stat,   8, "Status  (Port B)")            },
    { HRDATAD(CMD_B,  u.port[1].cmd,    8, "Command (Port B)")            },
    { HRDATAD(DATA_B, u.port[1].buf,    8, "Data    (Port B)")            },
    { NULL }
};

DEVICE uart_dev = {
    "UART", &uart_unit, uart_reg, NULL,
    1, 8, 32, 1, 8, 8,
    NULL, NULL, &uart_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat uart_reset(DEVICE *dptr)
{
    memset(&u, 0, sizeof(struct uart_state));

    u.c_en = FALSE;

    if (!sim_is_active(&uart_unit)) {
        uart_unit.wait = sim_rtcn_init(CLK_DELAY, CLK_UART);
        sim_activate(&uart_unit, uart_unit.wait);
    }

    return SCPE_OK;
}

t_stat uart_svc(UNIT *uptr)
{
    int32 temp;

    /* Recalibrate the timer */
    uart_unit.wait = sim_rtcn_calb(UART_HZ, CLK_UART);
    sim_activate(&uart_unit, uart_unit.wait);

    if (u.c_en) {
        u.c_val -= UART_SPC;
        if (u.c_val <= 0) {
            u.istat |= ISTS_CRI;
            u.c_val = u.c_set;

            /* This is a one-shot, so disable the timer. */
            /* TODO: Support square-wave mode. */
            u.c_en = FALSE;

            if (csr_data & CSRPIR9) {
                /* Set IRQ 9 */
                /* Flag the CSR with the source of the interrupt */
                sim_debug(EXECUTE_MSG, &uart_dev,
                          ">>> Setting IRQ9 on PIR9 UART TIMEOUT. mode=%02x, csr_data=%04x\n",
                          (u.acr >> 4) & 0x3, csr_data);
                csr_data |= CSRUART;
                cpu_set_irq(9, 9, 0);
            }
        }
    }

    if ((temp = sim_poll_kbd()) < SCPE_KFLAG) {
         return temp;
    }

    if (u.port[PORT_A].cmd & CMD_ETX) {
        uart_w_buf(PORT_A, temp);
        uart_update_rxi(temp);
    }

    return SCPE_OK;
}

/*
 *     Reg |       Name (Read)       |        Name (Write)
 *    -----+-------------------------+----------------------------
 *      0  | Mode Register A         | Mode Register A
 *      1  | Status Register A       | Clock Select Register A
 *      2  | BRG Test                | Command Register A
 *      3  | Rx Holding Register A   | Tx Holding Register A
 *      4  | Input Port Change Reg.  | Aux. Control Register
 *      5  | Interrupt Status Reg.   | Interrupt Mask Register
 *      6  | Counter/Timer Upper Val | C/T Upper Preset Val.
 *      7  | Counter/Timer Lower Val | C/T Lower Preset Val.
 *      8  | Mode Register B         | Mode Register B
 *      9  | Status Register B       | Clock Select Register B
 *     10  | 1X/16X Test             | Command Register B
 *     11  | Rx Holding Register B   | Tx Holding Register B
 *     12  | *Reserved*              | *Reserved*
 *     13  | Input Ports IP0 to IP6  | Output Port Conf. Reg.
 *     14  | Start Counter Command   | Set Output Port Bits Cmd.
 *     15  | Stop Counter Command    | Reset Output Port Bits Cmd.
 */

uint32 uart_read(uint32 pa, size_t size)
{
    uint8 reg;
    uint32 data;

    reg = pa - UARTBASE;

    switch (reg) {
    case 0:
        data = u.port[PORT_A].mode[u.port[PORT_A].mode_ptr];
        u.port[PORT_A].mode_ptr++;
        if (u.port[PORT_A].mode_ptr > 1) {
            u.port[PORT_A].mode_ptr = 0;
        }
        break;
    case 1:
        data = u.port[PORT_A].stat;
        u.port[PORT_A].stat &= ~STS_RXR;
        break;
    case 3:
        data = u.port[PORT_A].buf | (u.port[PORT_A].stat << 8);
        u.port[PORT_A].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RAI;
        break;
    case 5:
        data = u.istat;
        break;
    case 8:
        data = u.port[PORT_B].mode[u.port[PORT_B].mode_ptr];
        u.port[PORT_B].mode_ptr++;
        if (u.port[PORT_B].mode_ptr++ > 1) {
            u.port[PORT_B].mode_ptr = 0;
        }
        break;
    case 9:                                             /* status/clock B */
        data = u.port[PORT_B].stat;
        break;
    case 11:                                            /* tx/rx buf B */
        data = u.port[PORT_B].buf | (u.port[PORT_B].stat << 8);
        u.port[PORT_B].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RBI;
        break;
    case 14:
        /* Start Counter Command */
        sim_debug(EXECUTE_MSG, &uart_dev,
                  "[%08x] >>> STARTING UART TIMER! mode=%02x\n",
                  R[NUM_PC], (u.acr << 4) & 0x3);
        u.c_en = TRUE;
        break;
    case 15:
        /* Stop Counter Command */
        sim_debug(EXECUTE_MSG, &uart_dev,
                  "[%08x] >>> DISABLING UART TIMER!\n",
                  R[NUM_PC]);
        u.c_en = FALSE;
        u.istat &= ~ISTS_CRI;
        break;
    case 0x11: /* Clear DMAC interrupt */
        break;
    default:
        data = 0;
        break;
    }

    return data;
}

void uart_write(uint32 pa, uint32 val, size_t size)
{
    uint8 reg;
    uint8 mode_ptr;

    reg = pa - UARTBASE;

    switch (reg) {
    case 0:                /* Mode 1A, 2A */
        mode_ptr = u.port[PORT_A].mode_ptr;
        u.port[PORT_A].mode[mode_ptr++] = val & 0xff;
        if (mode_ptr > 1) {
            mode_ptr = 0;
        }
        u.port[PORT_A].mode_ptr = mode_ptr;
        break;
    case 1:
        /* Set baud rate - not implemented */
        break;
    case 2:  /* Command A */
        uart_w_cmd(PORT_A, val);
        uart_update_txi();
        break;
    case 3:  /* TX/RX Buf A */
        uart_w_buf(PORT_A, val);
        uart_update_txi();
        if (u.port[PORT_A].cmd & CMD_ETX) {
            /* TODO: This is probably not right, but let's do this for
               debugging / testing, and fix it later. */
            sim_putchar_s(u.port[PORT_A].buf);
        }
        break;
    case 4:  /* Auxiliary Control Register */
        u.acr = val;
        break;
    case 5:
        u.imask = val;
        break;
    case 6:  /* Counter/Timer Upper Preset Value */
        /* Clear out high byte */
        u.c_set &= 0x00ff;
        u.c_val &= 0x00ff;
        /* Set high byte */
        u.c_set |= (val & 0xff) << 8;
        u.c_val |= (val & 0xff) << 8;

        sim_debug(WRITE_MSG, &uart_dev, "CTU: set=%02x, New value=%04x\n", val, u.c_val);

        break;
    case 7:  /* Counter/Timer Lower Preset Value */
        /* Clear out low byte */
        u.c_set &= 0xff00;
        u.c_val &= 0xff00;
        /* Set low byte */
        u.c_set |= (val & 0xff);
        u.c_val |= (val & 0xff);

        sim_debug(WRITE_MSG, &uart_dev, "CTL:  set=%02x, New value=%04x\n", val, u.c_val);

        break;
    case 10: /* Command B */
        uart_w_cmd(PORT_B, val);
        uart_update_txi();
        break;
    case 11: /* TX/RX Buf B */
        uart_w_buf(PORT_B, val);
        uart_update_txi();
        break;
    case 0x11: /* Unknown register in the memory map */
        sim_debug(READ_MSG, &uart_dev, ">>> Unknown device at 49011, val=%02x\n", val);
        break;
    default:
        break;
    }
}

static SIM_INLINE void uart_update_txi()
{
    if (u.port[PORT_A].cmd & CMD_ETX) {                 /* Transmitter A enabled? */
        u.port[PORT_A].stat |= STS_TXR;                 /* ready */
        u.port[PORT_A].stat |= STS_TXE;                 /* empty */
        u.istat |= ISTS_TAI;                            /* set int */
    } else {
        u.port[PORT_A].stat &= ~STS_TXR;                /* clear ready */
        u.port[PORT_A].stat &= ~STS_TXE;                /* clear empty */
        u.istat &= ~ISTS_TAI;                           /* clear int */
    }

    if (u.port[PORT_B].cmd & CMD_ETX) {                 /* Transmitter B enabled? */
        u.port[PORT_B].stat |= STS_TXR;                 /* ready */
        u.port[PORT_B].stat |= STS_TXE;                 /* empty */
        u.istat |= ISTS_TBI;                            /* set int */
    } else {
        u.port[PORT_B].stat &= ~STS_TXR;                /* clear ready */
        u.port[PORT_B].stat &= ~STS_TXE;                /* clear empty */
        u.istat &= ~ISTS_TBI;                           /* clear int */
    }

    if (u.istat & u.imask) {                            /* unmasked ints? */
        /* TODO: Set interrupt */
        sim_debug(EXECUTE_MSG, &csr_dev,
                  ">>> Firing IRQ 9 via UART TX. ISTAT=%02x, IMASK=%02x\n", u.istat, u.imask);
        csr_data |= CSRUART;
        cpu_set_irq(9, 9, 0);
    } else {
        csr_data &= ~CSRUART;
        /* TODO: Clear interrupt */
    }

}

static SIM_INLINE void uart_update_rxi(uint8 c)
{
    if (u.port[PORT_A].cmd & CMD_ERX) {
        if (((u.port[PORT_A].stat & STS_RXR) == 0)) {
            u.port[PORT_A].buf = c;
            u.port[PORT_A].stat |= STS_RXR;
            u.istat |= ISTS_RAI;
        }
    } else {
        u.port[PORT_A].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RAI;
    }

    if (u.port[PORT_B].cmd & CMD_ERX) {
        if (((u.port[PORT_B].stat & STS_RXR) == 0)) {
            u.port[PORT_B].buf = c;
            u.port[PORT_B].stat |= STS_RXR;
            u.istat |= ISTS_RBI;
        }
    } else {
        u.port[PORT_B].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RBI;
    }
}

static SIM_INLINE void uart_w_buf(uint8 portno, uint8 val)
{
    u.port[portno].buf = val;
    u.port[portno].stat |= STS_RXR;
    switch (portno) {
    case PORT_A:
        u.istat |= ISTS_RAI;
        break;
    case PORT_B:
        u.istat |= ISTS_RBI;
        break;

    }
}

static SIM_INLINE void uart_w_cmd(uint8 portno, uint8 val)
{
    /* Enable or disable transmitter */
    if (val & CMD_ETX) {
        u.port[portno].cmd |= CMD_ETX;
    } else if (val & CMD_DTX) {
        u.port[portno].cmd &= ~CMD_ETX;
    }

    /* Enable or disable receiver */
    if (val & CMD_ERX) {
        u.port[portno].cmd |= CMD_ERX;
    } else if (val & CMD_DRX) {
        u.port[portno].cmd &= ~CMD_ERX;
    }

    switch ((val >> CMD_V_CMD) & CMD_M_CMD) {
    case 1:
        u.port[portno].mode_ptr = 0;
        break;
    case 2:
        u.port[portno].cmd &= ~CMD_ERX;
        u.port[portno].stat &= ~STS_RXR;
        break;
    case 3:
        u.port[portno].stat &= ~STS_TXR;
        break;
    case 4:
        u.port[portno].stat &= ~(STS_FER | STS_PER | STS_OER);
        break;
    }
}