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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#ifdef _KERNEL_OPT
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_clk.h>

static const struct device_compatible_entry compat_data[] = {
	{ .compat = "spacemit,k1x-clock", },
	DEVICE_COMPAT_EOL,
};

enum st_clk_handle {
	ST_CLK_HANDLE_MPMU,
	ST_CLK_HANDLE_APMU,
	ST_CLK_HANDLE_APBC,
	ST_CLK_HANDLE_APBS,
	ST_CLK_HANDLE_CIU,
	ST_CLK_HANDLE_DCIU,
	ST_CLK_HANDLE_DDRC,
	ST_CLK_HANDLE_APBC2,
	ST_CLK_HANDLE_RCPU,
	ST_CLK_HANDLE_RCPU2,
	ST_CLK_NHANDLES,
};

#define	PLL_TABLE_ENTRY_MHz(mhz, ctl, div)				\
    ST_CLK_PLL_TABLE_ENTRY((u_int)((mhz) * 1000 * 1000), ctl, div)

static const struct st_clk_pll_table
	pll_1600p0 = PLL_TABLE_ENTRY_MHz(1600.0, 0x0050cd61, 0x43eaaaab),
	pll_1800p0 = PLL_TABLE_ENTRY_MHz(1800.0, 0x0050cd61, 0x4b000000),
	pll_2000p0 = PLL_TABLE_ENTRY_MHz(2000.0, 0x0050dd62, 0x2aeaaaab),
	pll_2457p6 = PLL_TABLE_ENTRY_MHz(2457.6, 0x0050dd64, 0x330ccccd),
	pll_2800p0 = PLL_TABLE_ENTRY_MHz(2800.0, 0x0050dd66, 0x3a155555),
	pll_3000p0 = PLL_TABLE_ENTRY_MHz(3000.0, 0x0050dd66, 0x3fe00000),
	pll_3200p0 = PLL_TABLE_ENTRY_MHz(3200.0, 0x0050dd67, 0x43eaaaab);

static const struct st_clk_pll_table pll2_table[] = {
	pll_2457p6,
	pll_2800p0,
	pll_3000p0,
	pll_3200p0,
	ST_CLK_PLL_TABLE_EOL,
};

static const struct st_clk_pll_table pll3_table[] = {
	pll_1600p0,
	pll_1800p0,
	pll_2000p0,
	pll_2457p6,
	pll_3000p0,
	pll_3200p0,
	ST_CLK_PLL_TABLE_EOL,
};

static const char *uart_common_parents[3] = {
	[0] = "pll1_m3d128_57p6",
	[1] = "slow_uart1_14p74",
	[2] = "slow_uart2_48",
};

static const char *pwm_common_parents[2] = {
	[0] = "pll1_d192_12p8",
	[1] = "clk_32k",
};

static const char *twsi_common_parents[3] = {
	[0] = "pll1_d78_31p5",
	[1] = "pll1_d48_51p2",
	[2] = "pll1_d40_61p44",
};

static const char *timer_common_parents[5] = {
	[0] = "pll1_d192_12p8",
	[1] = "clk_32k",
	[2] = "pll1_d384_6p4",
	[3] = "vctcxo_3",
	[4] = "vctcxo_1",
};

static const char *sspa_common_parents[8] = {
	[0] = "pll1_d384_6p4",
	[1] = "pll1_d192_12p8",
	[2] = "pll1_d96_25p6",
	[3] = "pll1_d48_51p2",
	[4] = "pll1_d768_3p2",
	[5] = "pll1_d1536_1p6",
	[6] = "pll1_d3072_0p8",
	[7] = "i2s_bclk",
};

static const char *ccic_phy_common_parents[2] = {
	[0] = "pll1_d24_102p4",
	[1] = "pll1_d48_51p2_ap",
};

static const char *camm_common_parents[4] = {
	[0] = "pll1_d8_307p2",
	[1] = "pll2_d5",
	[2] = "pll1_d6_409p6",
	[3] = "vctcxo_24",
};

static const char *ssp3_clk_parents[7] = {
	[0] = "pll1_d384_6p4",
	[1] = "pll1_d192_12p8",
	[2] = "pll1_d96_25p6",
	[3] = "pll1_d48_51p2",
	[4] = "pll1_d768_3p2",
	[5] = "pll1_d1536_1p6",
	[6] = "pll1_d3072_0p8",
};

static const char *can0_clk_parents[3] = {
	[0] = "pll3_20",
	[1] = "pll3_40",
	[2] = "pll3_80",
};

static const char *jpg_clk_parents[7] = {
	[0] = "pll1_d4_614p4",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d5_491p52",
	[3] = "pll1_d3_819p2",
	[4] = "pll1_d2_1228p8",
	[5] = "pll2_d4",
	[6] = "pll2_d3",
};

static const char *csi_clk_parents[8] = {
	[0] = "pll1_d5_491p52",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d3_819p2",
	[4] = "pll2_d2",
	[5] = "pll2_d3",
	[6] = "pll2_d4",
	[7] = "pll1_d2_1228p8",
};

static const char *isp_cpp_clk_parents[2] = {
	[0] = "pll1_d8_307p2",
	[1] = "pll1_d6_409p6",
};

static const char *isp_bus_clk_parents[4] = {
	[0] = "pll1_d6_409p6", 
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d8_307p2",
	[3] = "pll1_d10_245p76",
};

static const char *isp_clk_parents[4] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d8_307p2",
};

static const char *dpu_mclk_parents[4] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d8_307p2",
};

static const char *dpu_esc_clk_parents[4] = {
	[0] = "pll1_d48_51p2_ap",
	[1] = "pll1_d52_47p26",
	[2] = "pll1_d96_25p6",
	[3] = "pll1_d32_76p8",
};

// XXX 6 should be 429M?
static const char *dpu_bit_clk_parents[8] = {
	[0] = "pll1_d3_819p2",
	[1] = "pll2_d2",
	[2] = "pll2_d3",
	[3] = "pll1_d2_1228p8",
	[4] = "pll2_d4",
	[5] = "pll2_d5",
	[6] = "pll2_d8",
	[7] = "pll2_d8",
};

static const char *dpu_pxclk_parents[6] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d8_307p2",
	[4] = "pll2_d7",
	[5] = "pll2_d8",
};

static const char *dpu_spi_clk_parents[8] = {
	[0] = "pll1_d8_307p2",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d10_245p76",
	[3] = "pll1_d11_223p4",
	[4] = "pll1_d13_189",
	[5] = "pll1_d23_106p8",
	[6] = "pll2_d3",
	[7] = "pll2_d5",
};

static const char *v2d_clk_parents[4] = {
	[0] = "pll1_d5_491p52",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d8_307p2",
	[3] = "pll1_d4_614p4",
};

#if 0 // XXXRO
static const char *ccic_4x_clk_parents[8] = {
	[0] = "pll1_d5_491p52",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d3_819p2",
	[4] = "pll2_d2",
	[5] = "pll2_d3",
	[6] = "pll2_d4",
	[7] = "pll1_d2_1228p8",
};
#else
static const char *ccic_4x_clk_parents[4] = {
	[0] = "pll1_d5_491p52",
	[1] = "pll1_d6_409p6",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d3_819p2",
};
#endif

static const char *ccic1phy_clk_parents[2] = {
	[0] = "pll1_d24_102p4",
	[1] = "pll1_d48_51p2_ap",
};

static const char *sdh01_common_parents[7] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d4_614p4",
	[2] = "pll2_d8",
	[3] = "pll2_d5",
	[4] = "pll1_d11_223p4",
	[5] = "pll1_d13_189",
	[6] = "pll1_d23_106p8",
};
#define	sdh0_clk_parents sdh01_common_parents
#define	sdh1_clk_parents sdh01_common_parents

static const char *sdh2_clk_parents[7] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d4_614p4",
	[2] = "pll2_d8",
	[3] = "pll1_d3_819p2",
	[4] = "pll1_d11_223p4",
	[5] = "pll1_d13_189",
	[6] = "pll1_d23_106p8",
};

static const char *qspi_clk_parents[8] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll2_d8",
	[2] = "pll1_d8_307p2",
	[3] = "pll1_d10_245p76",
	[4] = "pll1_d11_223p4",
	[5] = "pll1_d23_106p8",
	[6] = "pll1_d5_491p52",
	[7] = "pll1_d13_189",
};

static const char *aes_clk_parents[2] = {
	[0] = "pll1_d12_204p8",
	[1] = "pll1_d24_102p4",
};

static const char *vpu_gpu_common_parents[8] = {
	[0] = "pll1_d4_614p4",
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d3_819p2",
	[3] = "pll1_d6_409p6",
	[4] = "pll3_d6",
	[5] = "pll2_d3",
	[6] = "pll2_d4",
	[7] = "pll2_d5",
};
#define	vpu_clk_parents vpu_gpu_common_parents
#define	gpu_clk_parents vpu_gpu_common_parents

static const char *emmc_clk_parents[4] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d4_614p4",
	[2] = "pll1_d52_47p26",
	[3] = "pll1_d3_819p2",
};

static const char *audio_clk_parents[3] = {
	[0] = "pll1_aud_245p7",
	[1] = "pll1_d8_307p2",
	[2] = "pll1_d6_409p6",
};

static const char *hdmi_mclk_parents[4] = {
	[0] = "pll1_d6_409p6",
	[1] = "pll1_d5_491p52",
	[2] = "pll1_d4_614p4",
	[3] = "pll1_d8_307p2",
};

static const char *cci550_clk_parents[4] = {
	[0] = "pll1_d5_491p52",
	[1] = "pll1_d4_614p4",
	[2] = "pll1_d3_819p2",
	[3] = "pll2_d3",
};

static const char *pmua_aclk_parents[2] = {
	[0] = "pll1_d10_245p76",
	[1] = "pll1_d8_307p2",
};

static const char *cpu_c01_hi_common_parents[2] = {
	[0] = "pll3_d2",
	[1] = "pll3_d1",
};
#define	cpu_c0_hi_clk_parents cpu_c01_hi_common_parents
#define	cpu_c1_hi_clk_parents cpu_c01_hi_common_parents

static const char *cpu_c0_core_clk_parents[8] = {
	[0] = "pll1_d4_614p4",
	[1] = "pll1_d3_819p2",
	[2] = "pll1_d6_409p6",
	[3] = "pll1_d5_491p52",
	[4] = "pll1_d2_1228p8",
	[5] = "pll3_d3",
	[6] = "pll2_d3",
	[7] = "cpu_c0_hi_clk",
};

static const char *cpu_c1_pclk_parents[] = {
	[0] = "pll1_d4_614p4",
	[1] = "pll1_d3_819p2",
	[2] = "pll1_d6_409p6",
	[3] = "pll1_d5_491p52",
	[4] = "pll1_d2_1228p8",
	[5] = "pll3_d3",
	[6] = "pll2_d3",
	[7] = "cpu_c1_hi_clk",
};

static const char *uart1_sec_clk_parents[3] = {
	[0] = "pll1_m3d128_57p6",
	[1] = "slow_uart1_14p74",
	[2] = "slow_uart2_48",
};

static const char *ssp2_sec_clk_parents[7] = {
	[0] = "pll1_d384_6p4",
	[1] = "pll1_d192_12p8",
	[2] = "pll1_d96_25p6",
	[3] = "pll1_d48_51p2",
	[4] = "pll1_d768_3p2",
	[5] = "pll1_d1536_1p6",
	[6] = "pll1_d3072_0p8",
};

static const char *twsi3_sec_clk_parents[3] = {
	[0] = "pll1_d78_31p5",
	[1] = "pll1_d48_51p2",
	[2] = "pll1_d40_61p44",
};

static const char *timers0_kpc_sec_common_parents[5] = {
	[0] = "pll1_d192_12p8",
	[1] = "clk_32k",
	[2] = "pll1_d384_6p4",
	[3] = "vctcxo_3",
	[4] = "vctcxo_1",
};
#define	timers0_sec_clk_parents	timers0_kpc_sec_common_parents
#define	kpc_sec_clk_parents	timers0_kpc_sec_common_parents

static const char *apb_clk_parents[4] = {
	[0] = "pll1_d96_25p6",
	[1] = "pll1_d48_51p2",
	[2] = "pll1_d96_25p6",
	[3] = "pll1_d24_102p4",
};

static const char *rhdmi_audio_clk_parents[2] = {
	[0] = "pll1_aud_24p5",
	[1] = "pll1_aud_245p7",
};

static const char *rcan_clk_parents[3] = {
	[0] = "pll3_20",
	[1] = "pll3_40",
	[2] = "pll3_80",
};

static const char *rpwm_clk_parents[2] = {
	[0] = "pll1_aud_24p5",
	[1] = "pll1_aud_245p7",
};

static struct st_clk_clk st_k1x_clks[] = {

#define	PLL(id, name, ctl, lock_bit, table)				\
    ST_CLK_PLL_ENTRY(id, name,						\
	ST_CLK_HANDLE_APBS, ctl, /*sel*/(ctl) + 0x4, /*xtc*/(ctl) + 0x8,\
	ST_CLK_HANDLE_MPMU, /*lock*/0x10, __BIT(lock_bit), table)

	PLL(0, pll2, 0x118, 28, pll2_table),
	PLL(1, pll3, 0x124, 29, pll3_table),

#define	GF(id, name, parent, handle, ctl, gate_mask, factor_div, factor_mult) \
     ST_CLK_MIX_ENTRY(id, name,						\
	ST_CLK_SINGLE_PARENTS(parent), 1,				\
	ST_CLK_HANDLE_ ## handle, ctl, /*sel*/ctl,			\
	/*div*/0, /*fc*/0, /*pid*/0, gate_mask,				\
	factor_div, factor_mult)

#define	GF_PLL1(id, name, gate_bit, factor_div)				\
    GF(id, name, "pll1_2457p6_vco", APBS, 0x104, __BIT(gate_bit), factor_div, 1)

	GF_PLL1( 2, pll1_d2,	     1,   2),
	GF_PLL1( 3, pll1_d3,	     2,   3),
	GF_PLL1( 4, pll1_d4,	     3,   4),
	GF_PLL1( 5, pll1_d5,	     4,   5),
	GF_PLL1( 6, pll1_d6,	     5,   6),
	GF_PLL1( 7, pll1_d7,	     6,   7),
	GF_PLL1( 8, pll1_d8,	     7,   8),
	GF_PLL1( 9, pll1_d11_223p4, 15,  11),
	GF_PLL1(10, pll1_d13_189,   16,  13),
	GF_PLL1(11, pll1_d23_106p8, 20,  23),
	GF_PLL1(12, pll1_d64_38p4,   0,  64),
	GF_PLL1(13, pll1_aud_245p7, 10,  10),
	GF_PLL1(14, pll1_aud_24p5,  11, 100),

#define	GF_PLL2(id, name, gate_bit)					\
    GF(id, name, "pll2", APBS, 0x11c, __BIT(gate_bit), /*div*/(gate_bit) + 1, 1)

	GF_PLL2(15, pll2_d1, 0),
	GF_PLL2(16, pll2_d2, 1),
	GF_PLL2(17, pll2_d3, 2),
	GF_PLL2(18, pll2_d4, 3),
	GF_PLL2(19, pll2_d5, 4),
	GF_PLL2(20, pll2_d6, 5),
	GF_PLL2(21, pll2_d7, 6),
	GF_PLL2(22, pll2_d8, 7),

#define	GF_PLL3(id, name, gate_bit)					\
    GF(id, name, "pll3", APBS, 0x128, __BIT(gate_bit), /*div*/(gate_bit) + 1, 1)

	GF_PLL3(23, pll3_d1, 0),
	GF_PLL3(24, pll3_d2, 1),
	GF_PLL3(25, pll3_d3, 2),
	GF_PLL3(26, pll3_d4, 3),
	GF_PLL3(27, pll3_d5, 4),
	GF_PLL3(28, pll3_d6, 5),
	GF_PLL3(29, pll3_d7, 6),
	GF_PLL3(30, pll3_d8, 7),

#define	GF_PLL1D(id, name, parent, gate_bit, factor_div, factor_mult)	\
    GF(id, name, parent, MPMU, 0x1024, __BIT(gate_bit), factor_div, factor_mult)

// XXX introduce FF clock?
#define	F(id, name, parent, factor_div, factor_mult)			\
    ST_CLK_MIX_ENTRY(id, name,						\
	ST_CLK_SINGLE_PARENTS(parent), 1,				\
	/*handle*/0, /*ctl*/0, /*sel*/0,				\
	/*div*/0, /*fc*/0, /*pid*/0, /*gate*/0,				\
	factor_div, factor_mult)

	GF_PLL1D(31, pll1_d8_307p2,	 "pll1_d8",	  13,  1, 1),
	       F(32, pll1_d32_76p8,	 "pll1_d8_307p2",      4, 1),
	       F(33, pll1_d40_61p44,	 "pll1_d8_307p2",      5, 1),
	F(       34, pll1_d16_153p6,	 "pll1_d8",	       2, 1),
	GF_PLL1D(35, pll1_d24_102p4,	 "pll1_d8",	  12,  3, 1),
	GF_PLL1D(36, pll1_d48_51p2,	 "pll1_d8",	   7,  6, 1),
	GF_PLL1D(37, pll1_d48_51p2_ap,	 "pll1_d8",	  11,  6, 1),
	GF_PLL1D(38, pll1_m3d128_57p6,	 "pll1_d8",	   8, 16, 3),
	GF_PLL1D(39, pll1_d96_25p6,	 "pll1_d8",	   4, 12, 1),
	GF_PLL1D(40, pll1_d192_12p8,	 "pll1_d8",	   3, 24, 1),
	GF_PLL1D(41, pll1_d192_12p8_wdt, "pll1_d8",	  19, 24, 1),
	GF_PLL1D(42, pll1_d384_6p4,	 "pll1_d8",	   2, 48, 1),
	       F(43, pll1_d768_3p2,	 "pll1_d384_6p4",      2, 1),
	       F(44, pll1_d1536_1p6,	 "pll1_d384_6p4",      4, 1),
	       F(45, pll1_d3072_0p8,	 "pll1_d384_6p4",      8, 1),
	F(	 46, pll1_d7_351p08,	 "pll1_d7",	       1, 1),
	GF_PLL1D(47, pll1_d6_409p6,	 "pll1_d6",	   0,  1, 1),
	GF_PLL1D(48, pll1_d12_204p8,	 "pll1_d6",	   5,  2, 1),
	GF_PLL1D(49, pll1_d5_491p52,	 "pll1_d5",	  21,  1, 1),
	GF_PLL1D(50, pll1_d10_245p76,	 "pll1_d5",	  18,  2, 1),
	GF_PLL1D(51, pll1_d4_614p4,	 "pll1_d4",	  15,  1, 1),
	GF_PLL1D(52, pll1_d52_47p26,	 "pll1_d4",	  10, 13, 1),
	GF_PLL1D(53, pll1_d78_31p5,	 "pll1_d4",	   6, 39, 2),
	GF_PLL1D(54, pll1_d3_819p2,	 "pll1_d3",	  14,  1, 1),
	GF_PLL1D(55, pll1_d2_1228p8,	 "pll1_d2",	  16,  1, 1),

#define	DDN_SU(id, name, parent, ctl, factor_div, factor_mult)		\
    ST_CLK_DDN_ENTRY(id, name, parent,					\
	ST_CLK_HANDLE_MPMU, ctl, /*sel*/0,				\
	/*gate*/0, /*div*/__BITS(16, 28), /*mult*/__BITS(0, 12),	\
	/*factor*/2, ST_CLK_SINGLE_DDN_TABLE(factor_div, factor_mult))

	DDN_SU(56, slow_uart1_14p74, "pll1_d16_153p6", 0x0014,  125,  24),
	DDN_SU(57, slow_uart2_48,    "pll1_d4_614p4",  0x10b0, 6144, 960),

#define	pMG(id, name, parents, handle, ctl, pid_mask, gate_mask)	\
    ST_CLK_MIX_ENTRY(id, name,						\
	parents, __arraycount(parents),					\
	ST_CLK_HANDLE_ ## handle, ctl, /*sel*/ctl,			\
	/*div*/0, /*fc*/0, pid_mask, gate_mask,				\
	/*factor*/1, 1)

#define	MG(id, name, handle, ctl, pid_mask, gate_mask)			\
    pMG(id, name, name ## _parents, handle, ctl, pid_mask, gate_mask)

#define	MG_UART(id, name, ctl)						\
    pMG(id, name, uart_common_parents, APBC, ctl, __BITS(4, 6), __BITS(0, 1))

	MG_UART(58, uart1_clk, 0x00),
	MG_UART(59, uart2_clk, 0x04),
	MG_UART(60, uart3_clk, 0x24),
	MG_UART(61, uart4_clk, 0x70),
	MG_UART(62, uart5_clk, 0x74),
	MG_UART(63, uart6_clk, 0x78),
	MG_UART(64, uart7_clk, 0x94),
	MG_UART(65, uart8_clk, 0x98),
	MG_UART(66, uart9_clk, 0x9c),

#define	G(id, name, parent, handle, ctl, gate_mask)			\
    ST_CLK_MIX_ENTRY(id, name,						\
	ST_CLK_SINGLE_PARENTS(parent), (parent) != NULL ? 1 : 0,	\
	ST_CLK_HANDLE_ ## handle, ctl, /*sel*/ctl,			\
	/*div*/0, /*fc*/0, /*pid*/0, gate_mask,				\
	/*factor*/1, 1)

	G(67, gpio_clk, "vctcxo_24", APBC, 0x08, __BITS(0, 1)),

#define	MG_PWM(id, name, ctl)						\
    pMG(id, name, pwm_common_parents, APBC, ctl, __BITS(4, 6), __BIT(1))

	MG_PWM(68,  pwm0_clk, 0x0c),
	MG_PWM(69,  pwm1_clk, 0x10),
	MG_PWM(70,  pwm2_clk, 0x14),
	MG_PWM(71,  pwm3_clk, 0x18),
	MG_PWM(72,  pwm4_clk, 0xa8),
	MG_PWM(73,  pwm5_clk, 0xac),
	MG_PWM(74,  pwm6_clk, 0xb0),
	MG_PWM(75,  pwm7_clk, 0xb4),
	MG_PWM(76,  pwm8_clk, 0xb8),
	MG_PWM(77,  pwm9_clk, 0xbc),
	MG_PWM(78, pwm10_clk, 0xc0),
	MG_PWM(79, pwm11_clk, 0xc4),
	MG_PWM(80, pwm12_clk, 0xc8),
	MG_PWM(81, pwm13_clk, 0xcc),
	MG_PWM(82, pwm14_clk, 0xd0),
	MG_PWM(83, pwm15_clk, 0xd4),
	MG_PWM(84, pwm16_clk, 0xd8),
	MG_PWM(85, pwm17_clk, 0xdc),
	MG_PWM(86, pwm18_clk, 0xe0),
	MG_PWM(87, pwm19_clk, 0xe4),

	MG(88, ssp3_clk, APBC, 0x7c, __BITS(4, 6), __BITS(0, 1)),

	G(89, rtc_clk, "clk_32k", APBC, 0x28, 0x83),

#define	MG_TWSI(id, name, ctl)						\
    pMG(id, name, twsi_common_parents, APBC, ctl, __BITS(4, 6), __BITS(0, 1))

	MG_TWSI(90, twsi0_clk, 0x2c),
	MG_TWSI(91, twsi1_clk, 0x30),
	MG_TWSI(92, twsi2_clk, 0x38),
	// twsi3_clk is missing!
	MG_TWSI(93, twsi4_clk, 0x40),
	MG_TWSI(94, twsi5_clk, 0x4c),
	MG_TWSI(95, twsi6_clk, 0x60),
	MG_TWSI(96, twsi7_clk, 0x68),
	MG_TWSI(97, twsi8_clk, 0x20),

// "twsi8_clk", QUIRK_MIX_GATE_DISABLE_MASK, 0x4

#define	MG_TIMER(id, name, ctl)						\
    pMG(id, name, timer_common_parents, APBC, ctl, __BITS(4, 6), __BITS(0, 1))

	MG_TIMER(98, timers1_clk, 0x34),
	MG_TIMER(99, timers2_clk, 0x44),

	G(100, aib_clk,     "vctcxo_24", APBC, 0x3c, __BITS(0, 1)),
	G(101, onewire_clk, NULL,        APBC, 0x48, __BITS(0, 1)),

#define	MG_SSPA(id, name, ctl)						\
    pMG(id, name, sspa_common_parents, APBC, ctl, __BITS(4, 6), __BITS(0, 1))

	MG_SSPA(102, sspa0_clk, 0x80),
	MG_SSPA(103, sspa1_clk, 0x84),

	 G(104, dro_clk,	NULL, APBC, 0x58, __BIT( 0)),
	 G(105, ir_clk,		NULL, APBC, 0x5c, __BIT( 0)),
	 G(106, tsen_clk,	NULL, APBC, 0x6c, __BITS(0, 1)),
	 G(107, ipc_ap2aud_clk,	NULL, APBC, 0x90, __BITS(0, 1)),

// XXXRO U-Boot: can0_clk <- pll1_m3d128_57p6 (not present in can0_clk_parents)
	MG(108, can0_clk,	      APBC, 0xa0, __BITS(4, 6), __BIT(1)),

	 G(109, can0_bus_clk,	NULL, APBC, 0xa0, __BIT(0)),

	 G(110, wdt_clk,  "pll1_d96_25p6", MPMU, 0x0200, __BITS(0, 1)),
	 G(111, ripc_clk, NULL,		   MPMU, 0x0210, __BITS(0, 1)),

#define	pDfMG(id, name, parents, handle, ctl, div_mask, fc_mask,	\
	pid_mask, gate_mask)						\
    ST_CLK_MIX_ENTRY(id, name,						\
	parents, __arraycount(parents),					\
	ST_CLK_HANDLE_ ## handle, ctl, ctl,				\
	div_mask, fc_mask, pid_mask, gate_mask,				\
	/*factor*/0, 0)

#define	DfMG(id, name, handle, ctl, div_mask, fc_mask, pid_mask, gate_mask) \
    pDfMG(id, name, name ## _parents, handle, ctl, div_mask, fc_mask,	\
	pid_mask, gate_mask)

	DfMG(112, jpg_clk, APMU, 0x20, __BITS(5, 7), __BIT(15),
	    __BITS(2, 4), __BIT(1)),
	G(113, jpg_4kafbc_clk, NULL, APMU, 0x20, __BIT(16)),
	G(114, jpg_2kafbc_clk, NULL, APMU, 0x20, __BIT(17)),

#define	MG_CCIC_PHY(id, name, pid_bit, gate_bit)			\
    pMG(id, name, ccic_phy_common_parents, APMU, 0x24, __BIT(pid_bit),	\
	__BIT(gate_bit))

	MG_CCIC_PHY(115, ccic2phy_clk,  7,  5),
	MG_CCIC_PHY(116, ccic3phy_clk, 31, 30),

	DfMG(117, csi_clk, APMU, 0x24, __BITS(20, 22), __BIT(15),
	    __BITS(16, 18), __BIT(4)),

#define	DfMG_CAMM(id, name, gate_bit)					\
    pDfMG(id, name, camm_common_parents, APMU, 0x24, __BITS(23, 26), 0,	\
	__BITS(8, 9), __BIT(gate_bit))

	DfMG_CAMM(118, camm0_clk, 28),
	DfMG_CAMM(119, camm1_clk,  6),
	DfMG_CAMM(120, camm2_clk,  3),

	DfMG(121, isp_cpp_clk, APMU, 0x38, __BITS(24, 25), 0,
	    __BIT(26), __BIT(28)),
	DfMG(122, isp_bus_clk, APMU, 0x38, __BITS(18, 20), __BIT(23),
	    __BITS(21, 22), __BIT(17)),
	DfMG(123, isp_clk,     APMU, 0x38, __BITS( 4,  6), __BIT( 7),
	    __BITS(8, 9), __BIT(1)),

#define	sDfMG(id, name, handle, ctl, sel, div_mask, fc_mask, pid_mask,	\
	gate_mask)							\
    ST_CLK_MIX_ENTRY(id, name,						\
	name ## _parents, __arraycount(name ## _parents),		\
	ST_CLK_HANDLE_ ## handle, ctl, sel,				\
	div_mask, fc_mask, pid_mask, gate_mask,				\
	/*factor*/0, 0)

	sDfMG(124, dpu_mclk, APMU, 0x44, 0x4c, __BITS(1, 4), __BIT(29),
	    __BITS(5, 7), __BIT(0)),
	   MG(125, dpu_esc_clk, APMU, 0x44, __BITS(0, 1), __BIT(2)),
	 DfMG(126, dpu_bit_clk, APMU, 0x44, __BITS(17, 19), __BIT(31),
	    __BITS(20, 22), __BIT(16)),
	sDfMG(127, dpu_pxclk, APMU, 0x44, 0x4c, __BITS(17, 20), __BIT(30),
	    __BITS(21, 23), __BIT(16)),
	    G(128, dpu_hclk,	     NULL, APMU, 0x44, __BIT(5)),
	 DfMG(129, dpu_spi_clk, APMU, 0x48, __BITS(8, 10), __BIT(7),
	    __BITS(12, 14), __BIT(1)),
	    G(130, dpu_spi_hbus_clk, NULL, APMU, 0x48, __BIT(3)),
	    G(131, dpu_spi_bus_clk,  NULL, APMU, 0x48, __BIT(5)),
	    G(132, dpu_spi_aclk,     NULL, APMU, 0x48, __BIT(6)),
	 DfMG(133, v2d_clk,	 APMU, 0x44, __BITS( 9, 11), __BIT(28),
	    __BITS(12, 13), __BIT(8)),
	 DfMG(134, ccic_4x_clk,  APMU, 0x50, __BITS(18, 20), __BIT(15),
	    __BITS(23, 24), __BIT(4)),
	   MG(135, ccic1phy_clk, APMU, 0x50, __BIT(7), __BIT(5)),

#define	DfMG_SDH(id, name, ctl)						\
    DfMG(id, name, APMU, ctl, __BITS(8, 10), __BIT(11), __BITS(5, 7), __BIT(4))

	G(136, sdh_axi_aclk, NULL, APMU, 0x54, __BIT(3)),

	DfMG_SDH(137, sdh0_clk, 0x54),
	DfMG_SDH(138, sdh1_clk, 0x58),
	DfMG_SDH(139, sdh2_clk, 0xe0),

#define	G_USB(id, name, gate_bit)					\
    G(id, name, NULL, APMU, 0x5c, __BIT(gate_bit))

	G_USB(140, usb_axi_clk, 1),
	G_USB(141, usb_p1_aclk, 5),
	G_USB(142, usb30_clk,   8),

	DfMG(143, qspi_clk,	      APMU, 0x60, __BITS( 9, 11), 0,
	    __BITS( 6,  8), __BIT(4)),
	   G(144, qspi_bus_clk, NULL, APMU, 0x60, __BIT(3)),
	   G(145, dma_clk,      NULL, APMU, 0x64, __BIT(3)),
	  MG(146, aes_clk,	      APMU, 0x68, __BIT(6), __BIT(5)),
	DfMG(147, vpu_clk,	      APMU, 0xa4, __BITS(13, 15), __BIT(21),
	    __BITS(10, 12), __BIT(3)),
	DfMG(148, gpu_clk,	      APMU, 0xcc, __BITS(12, 14), __BIT(15),
	    __BITS(18, 20), __BIT(4)),

#define	DG(id, name, parent, handle, ctl, div_mask, gate_mask)		\
    ST_CLK_MIX_ENTRY(id, name,						\
	ST_CLK_SINGLE_PARENTS(parent), 1,				\
	ST_CLK_HANDLE_ ## handle, ctl, ctl,				\
	div_mask, /*fc*/0, /*pid*/0, gate_mask,				\
	/*factor*/0, 0)

	DfMG(149, emmc_clk,  APMU, 0x104, __BITS(8, 10), __BIT(11),
	    __BITS(6, 7), __BITS(3, 4)),
	DG(  150, emmc_x_clk, "pll1_d2_1228p8", APMU, 0x104, __BITS(12, 14),
	    __BIT(15)),
	DfMG(151, audio_clk, APMU, 0x14c, __BITS(4,  6), __BIT(15),
	    __BITS(7, 9), __BIT(12)),
	DfMG(152, hdmi_mclk, APMU, 0x1b8, __BITS(1,  4), __BIT(29),
	    __BITS(5, 7), __BIT( 0)),

#define	DfM(id, name, handle, ctl, div_mask, fc_mask, pid_mask)		\
    ST_CLK_MIX_ENTRY(id, name,						\
	name ## _parents, __arraycount(name ## _parents),		\
	ST_CLK_HANDLE_ ## handle, ctl, ctl,				\
	div_mask, fc_mask, pid_mask, /*gate*/0,				\
	/*factor*/0, 0)

	DfM(153, cci550_clk, APMU, 0x300, __BITS(8, 10), __BIT(12),
	    __BITS(0, 1)),
	DfM(154, pmua_aclk,  APMU, 0x388, __BITS(1,  2), __BIT( 4),
	    __BIT( 0)),

#define	fM(id, name, handle, ctl, fc_mask, pid_mask)			\
    ST_CLK_MIX_ENTRY(id, name,						\
	name ## _parents, __arraycount(name ## _parents),		\
	ST_CLK_HANDLE_ ## handle, ctl, ctl,				\
	/*div*/0, fc_mask, pid_mask, /*gate*/0,				\
	/*factor*/1, 1)

#define	D(id, name, parents, handle, ctl, div_mask)			\
    ST_CLK_MIX_ENTRY(id, name,						\
	ST_CLK_SINGLE_PARENTS(parents), 1,				\
	ST_CLK_HANDLE_ ## handle, ctl, ctl,				\
	div_mask, /*fc*/0, /*pid*/0, /*gate*/0,				\
	/*factor*/0, 0)

	fM(155, cpu_c0_hi_clk,   APMU, 0x38c,         0, __BIT(13)),
	fM(156, cpu_c0_core_clk, APMU, 0x38c, __BIT(12), __BITS(0, 2)),
	D(157, cpu_c0_ace_clk, "cpu_c0_core_clk", APMU, 0x38c, __BITS(6,  8)),
	D(158, cpu_c0_tcm_clk, "cpu_c0_core_clk", APMU, 0x38c, __BITS(9, 11)),

	fM(159, cpu_c1_hi_clk,	APMU, 0x390,         0, __BIT(13)),
	fM(160, cpu_c1_pclk,	APMU, 0x390, __BIT(12), __BITS(0, 2)),
	D(161, cpu_c1_ace_clk, "cpu_c1_pclk", APMU, 0x390, __BITS(6, 8)),

	G(162, pcie0_clk, NULL, APMU, 0x3cc, __BITS(0, 2)),
	G(163, pcie1_clk, NULL, APMU, 0x3d4, __BITS(0, 2)),
	G(164, pcie2_clk, NULL, APMU, 0x3dc, __BITS(0, 2)),

	G(165, emac0_bus_clk, NULL,	 APMU, 0x3e4, __BIT( 0)),

// XXXRO: U-Boot: emac0_ptp_clk <- pll1_d3_819p2
	G(166, emac0_ptp_clk, "pll2_d6", APMU, 0x3e4, __BIT(15)),

	G(167, emac1_bus_clk, NULL,	 APMU, 0x3ec, __BIT( 0)),

// XXXRO: U-Boot: emac0_ptp_clk <- pll1_d3_819p2
	G(168, emac1_ptp_clk, "pll2_d6", APMU, 0x3ec, __BIT(15)),

#define	MG_SEC(id, name, ctl)						\
    MG(id, name, APBC2, ctl, __BITS(4, 6), __BITS(0, 1))

	MG_SEC(169, uart1_sec_clk,   0x00),
	MG_SEC(170, ssp2_sec_clk,    0x04),
	MG_SEC(171, twsi3_sec_clk,   0x08),
	 G(    172, rtc_sec_clk,  "clk_32k",   APBC2, 0x0c, 0x83),
	MG_SEC(173, timers0_sec_clk, 0x10),
	MG_SEC(174, kpc_sec_clk,     0x14),
	 G(    175, gpio_sec_clk, "vctcxo_24", APBC2, 0x1c, __BITS(0, 1)),

	fM(176, apb_clk, MPMU, 0x1050, 0, __BITS(0, 1)),

#define	F_PLL3_80(id, name, factor_div)	F(id, name, "pll3_d8", factor_div, 1)

	F_PLL3_80(177, pll3_80,  5),
	F_PLL3_80(178, pll3_40, 10),
	F_PLL3_80(179, pll3_20, 20),

	G (180, slow_uart,  NULL,	     MPMU, 0x1024, __BIT( 1)),

	GF(181, i2s_sysclk, "pll1_d8_307p2", MPMU, 0x0044, __BIT(31), 200, 1),
	G (182, i2s_bclk,   "i2s_sysclk",    MPMU, 0x0044, __BIT(29)),

	DfMG(183, rhdmi_audio_clk,	 RCPU, 0x2044, __BITS(4, 14), 0,
	    __BITS(16, 17), __BITS(1, 2)),
	DfMG(184, rcan_clk,		 RCPU, 0x004c, __BITS(8, 18), 0,
	    __BITS( 4,  5), __BIT( 1)),
	G(185, rcan_bus_clk, NULL,	 RCPU, 0x004c, __BIT(2)),

	DfMG(186, rpwm_clk, RCPU2, 0x08, __BITS(8, 18), 0,
	    __BITS(4, 5), __BIT(1)),
};

static int
st_k1x_clk_twsi8_quirk(struct st_clk_clk *clk)
{
	KASSERT(clk->scc_type == ST_CLK_TYPE_MIX);
	struct st_clk_mix * const mix = &clk->scc_mix;

	// XXX sel register is write-only
	clk->scc_quirk_flags = ST_CLK_QUIRK_MIX_SEL_WRONLY;
	clk->scc_saved_reg = __BIT(2);

	// XXX finite value for gate-disable
	mix->gate_disable = __BIT(2);
	mix->gate_mask |= mix->gate_disable;

	return 0;
}

static const struct st_clk_quirk st_k1x_quirks[] = {
	{ "twsi8_clk", st_k1x_clk_twsi8_quirk, },
	{ NULL, NULL, }
};

static const char *st_k1x_clk_boot_enable[] = {
	"pll1_d8_307p2",
	"pll1_d6_409p6",
	"pll1_d5_491p52",
	"pll1_d4_614p4",
	"pll1_d3_819p2",
	"pll1_d2_1228p8",
	"pll1_d10_245p76",
	"pll1_d48_51p2",
	"pll1_d48_51p2_ap",
	"pll1_d96_25p6",
	"pll3_d1",
	"pll3_d2",
	"pll3_d3",
	"pll2_d3",
	"apb_clk",
	"pmua_aclk",
	NULL,
};

static int
st_k1x_clk_match(device_t parent, cfdata_t cf, void *aux)
{
	struct fdt_attach_args * const faa = aux;

	return of_compatible_match(faa->faa_phandle, compat_data);
}

static void
st_k1x_clk_attach(device_t parent, device_t self, void *aux)
{
	struct st_clk_softc * const sc = device_private(self);
	struct fdt_attach_args * const faa = aux;

	sc->sc_dev = self;
	sc->sc_phandle = faa->faa_phandle;
	sc->sc_bst = faa->faa_bst;
	sc->sc_nbshs = ST_CLK_NHANDLES;
	sc->sc_clks = st_k1x_clks;
	sc->sc_nclks = __arraycount(st_k1x_clks);
	sc->sc_quirks = st_k1x_quirks;
	sc->sc_boot_enable = st_k1x_clk_boot_enable;

	st_clk_init(sc);
}

CFATTACH_DECL_NEW(st_k1x_clk, sizeof(struct st_clk_softc),
    st_k1x_clk_match, st_k1x_clk_attach, NULL, NULL);
