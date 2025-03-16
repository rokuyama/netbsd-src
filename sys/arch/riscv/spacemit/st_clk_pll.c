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
#include <sys/systm.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_backend.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_clk.h>

#define	PLL_RATE_MIN	( 600 * 1000)
#define	PLL_RATE_MAX	(3400 * 1000)

static uint32_t
st_clk_pll_lock_rd(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_pll *pll = &clk->scc_pll;

	return ST_CLK_RAW_RD(sc, pll->lock_handle, pll->lock_reg);
}

#if 0
static void
st_clk_pll_lock_wr(struct st_clk_softc *sc, struct st_clk_clk *clk,
    uint32_t val)
{
	const struct st_clk_pll *pll = &clk->scc_pll;

	ST_CLK_RAW_WR(sc, pll->lock_handle, pll->lock_reg, val);
}
#endif

static u_int
st_clk_pll_param2rate(struct st_clk_clk *clk, uint32_t ctl, uint32_t div)
{
	const struct st_clk_pll *pll = &clk->scc_pll;

	for (const struct st_clk_pll_table *table = pll->table;
	    table->rate != 0; table++) {
		if (table->ctl == ctl && table->div == div)
			return table->rate;
	}

	DPRINTF(CLK, "ctl 0x%08x, div 0x%08x: not found\n", ctl, div);
	return 0;
}

static int
st_clk_pll_rate2param(struct st_clk_clk *clk, u_int rate,
    uint32_t *ctl, uint32_t *div)
{
	const struct st_clk_pll *pll = &clk->scc_pll;

	for (const struct st_clk_pll_table *table = pll->table;
	    table->rate != 0; table++) {
		if (table->rate != rate)
			continue;

		*ctl = table->ctl;
		*div = table->div;
		return 0;
	}

	DPRINTF(CLK, "rate %u: not found\n", rate);
	return EINVAL;
}

static bool
st_clk_pll_is_enabled(struct st_clk_softc *sc, struct st_clk_clk *clk)
{

	ST_CLK_LOCK(sc);
	const uint32_t reg = ST_CLK_RD(sc, clk, XTC);
	ST_CLK_UNLOCK(sc);

	return (reg & ST_CLK_XTC_ENABLED) != 0;
}

static int
st_clk_pll_enable(struct st_clk_softc *sc, struct st_clk_clk *clk, bool enable)
{
	const struct st_clk_pll *pll = &clk->scc_pll;
	uint32_t xtc;
	int error = 0;

	ST_CLK_LOCK(sc);

	xtc = ST_CLK_RD(sc, clk, XTC);
	if (enable)
		xtc |= ST_CLK_XTC_ENABLED;
	else
		xtc &= ~ST_CLK_XTC_ENABLED;
	ST_CLK_WR(sc, clk, XTC, xtc);

	if (!enable)
		goto out;

	delay(50);
	for (u_int retry = 590; retry > 0; retry--) {
		const uint32_t lock = st_clk_pll_lock_rd(sc, clk);

		if ((lock & pll->lock_mask) != 0)
			goto out;

		delay(5);
	}
	error = ETIMEDOUT;

 out:
	ST_CLK_UNLOCK(sc);

	return error;
}

static u_int
st_clk_pll_get_rate(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const uint32_t ctl = ST_CLK_RD(sc, clk, CTL);
	const uint32_t div = ST_CLK_RD(sc, clk, XTC) & ST_CLK_XTC_DIV;

	return st_clk_pll_param2rate(clk, ctl, div);
}

static u_int
st_clk_pll_round_rate(struct st_clk_softc *sc, struct st_clk_clk *clk,
    u_int target, u_int *p0, u_int *p1)
{
	const struct st_clk_pll *pll = &clk->scc_pll;
	u_int best_rate = 0;

	if (target > PLL_RATE_MAX || target < PLL_RATE_MIN) {
		DPRINTF(CLK, "rate %u: out of range\n", target);
		return 0;
	}

	for (const struct st_clk_pll_table *table = pll->table;
	    table->rate != 0; table++) {
		const u_int rate = table->rate;

		if (rate <= target && rate > best_rate)
			best_rate = rate;
	}

	return best_rate;
}

static int
st_clk_pll_set_rate(struct st_clk_softc *sc, struct st_clk_clk *clk, u_int rate)
{
	const bool was_enabled = st_clk_is_enabled(sc, clk);
	uint32_t ctl, xtc;
	int error = 0;

	if (was_enabled)
		ST_CLK_DISABLE(clk);

	error = st_clk_pll_rate2param(clk, rate, &ctl, &xtc);
	if (error != 0)
		goto out;

	ST_CLK_LOCK(sc);
	ST_CLK_WR(sc, clk, CTL, ctl);
	ST_CLK_WR(sc, clk, XTC, xtc);
	ST_CLK_UNLOCK(sc);

 out:
	if (was_enabled)
		ST_CLK_ENABLE(clk);

	return error;
}

void
st_clk_pll_init(struct st_clk_clk *clk)
{
	const struct st_clk_pll *pll = &clk->scc_pll;
	struct st_clk_funcs * const fp = &clk->scc_funcs;

	DPRINTF(VERBOSE, "%u: %s\n", clk->scc_id, clk->scc_name);

	KASSERT(pll->lock_mask != 0);
	KASSERT(pll->table != NULL);

	fp->is_enabled = st_clk_pll_is_enabled;
	fp->enable = st_clk_pll_enable;
	fp->get_rate = st_clk_pll_get_rate;
	fp->round_rate = st_clk_pll_round_rate;
	fp->set_rate = st_clk_pll_set_rate;
}
