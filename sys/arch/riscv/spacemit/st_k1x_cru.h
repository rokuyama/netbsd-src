/*	$NetBSD$	*/

/*
 * Copyright (c) 2025 Rin Okuyama <rin@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RISCV_SPACEMIT_ST_K1X_CRU_H
#define _RISCV_SPACEMIT_ST_K1X_CRU_H

enum st_cru_handle {
	ST_CRU_HANDLE_MPMU,
	ST_CRU_HANDLE_APMU,
	ST_CRU_HANDLE_APBC,
	ST_CRU_HANDLE_APBS,
	ST_CRU_HANDLE_CIU,
	ST_CRU_HANDLE_DCIU,
	ST_CRU_HANDLE_DDRC,
	ST_CRU_HANDLE_APBC2,
	ST_CRU_HANDLE_RCPU,
	ST_CRU_HANDLE_RCPU2,
	ST_CRU_NHANDLES,
};

#define APBS_PLL2_CTL	0x118	/* SPARE7 */
#define APBS_PLL2_SEL	0x11c	/* SPARE8 */
#define APBS_PLL2_XTC	0x120	/* SPARE9 */

#define APBS_PLL3_CTL	0x124	/* SPARE10 */
#define APBS_PLL3_SEL	0x128	/* SPARE11 */
#define APBS_PLL3_XTC	0x12c	/* SPARE12 */

#define	APBS_PLL1_CTLSEL	0x104	/* SPARE2 */

#define MPMU_POSR	0x010	/* PLL[23] lock */
#define MPMU_ACGR	0x1024	/* GF_PLL1D */

#define MPMU_SUCCR0	0x0014	/* slow_uart1_14p74 */
#define MPMU_SUCCR1	0x10b0	/* slow_uart2_48 */

#define APBC_UART1	0x00
#define APBC_UART2	0x04
#define APBC_UART3	0x24
#define APBC_UART4	0x70
#define APBC_UART5	0x74
#define APBC_UART6	0x78
#define APBC_UART7	0x94
#define APBC_UART8	0x98
#define APBC_UART9	0x9c

#define APBC_GPIO	0x08

#define APBC_PWM0	0x0c
#define APBC_PWM1	0x10
#define APBC_PWM2	0x14
#define APBC_PWM3	0x18
#define APBC_PWM4	0xa8
#define APBC_PWM5	0xac
#define APBC_PWM6	0xb0
#define APBC_PWM7	0xb4
#define APBC_PWM8	0xb8
#define APBC_PWM9	0xbc
#define APBC_PWM10	0xc0
#define APBC_PWM11	0xc4
#define APBC_PWM12	0xc8
#define APBC_PWM13	0xcc
#define APBC_PWM14	0xd0
#define APBC_PWM15	0xd4
#define APBC_PWM16	0xd8
#define APBC_PWM17	0xdc
#define APBC_PWM18	0xe0
#define APBC_PWM19	0xe4

#define APBC_SSP3	0x7c

#define APBC_RTC	0x28	/* reserved */

#define APBC_TWSI0	0x2c
#define APBC_TWSI1	0x30
#define APBC_TWSI2	0x38
#define APBC_TWSI4	0x40
#define APBC_TWSI5	0x4c
#define APBC_TWSI6	0x60
#define APBC_TWSI7	0x68
#define APBC_TWSI8	0x20

#define APBC_TIMERS1	0x34
#define APBC_TIMERS2	0x44

#define APBC_AIB	0x3c
#define APBC_ONEWIRE	0x48

#define APBC_SSPA0	0x80
#define APBC_SSPA1	0x84

#define APBC_DRO	0x58
#define APBC_IR		0x5c
#define APBC_TSEN	0x6c
#define APBC_IPC_AP2AUD	0x90

#define APBC_CAN0	0xa0

#define MPMU_WDTPCR	0x200
#define MPMU_RIPCCR	0x210 //no define

#define APMU_JPG	0x20

#define APMU_CSI_CCIC2	0x24

#define APMU_ISP	0x38

#define APMU_LCD1	0x44
#define APMU_LCD_SPI	0x48
#define APMU_LCD2	0x4c
#define APMU_CCIC	0x50

#define APMU_SDH0	0x54
#define APMU_SDH1	0x58
#define APMU_SDH2	0xe0

#define APMU_USB	0x5c

#define APMU_QSPI	0x60
#define APMU_DMA	0x64
#define APMU_AES	0x68
#define APMU_VPU	0xa4
#define APMU_GPU	0xcc

#define APMU_PMUA_MC	0x0e8
#define APMU_PMUA_EM	0x104
#define APMU_AUDIO	0x14c
#define APMU_HDMI	0x1b8

#define APMU_CCI550	0x300
#define APMU_ACLK	0x388

#define APMU_CPU_C0	0x38c
#define APMU_CPU_C1	0x390

#define APMU_PCIE0	0x3cc
#define APMU_PCIE1	0x3d4
#define APMU_PCIE2	0x3dc

#define APMU_EMAC0	0x3e4
#define APMU_EMAC1	0x3ec

#define APBC2_UART1	0x00
#define APBC2_SSP2	0x04
#define APBC2_TWSI3	0x08
#define APBC2_RTC	0x0c
#define APBC2_TIMERS0	0x10
#define APBC2_KPC	0x14
#define APBC2_GPIO	0x1c

#define MPMU_APBCSCR	0x1050

#define MPMU_ISCCR	0x44

#define RCPU_HDMI	0x2044
#define RCPU_CAN	0x004c

#define RCPU2_PWM	0x08

#endif /* _RISCV_SPACEMIT_ST_K1X_CRU_H */
