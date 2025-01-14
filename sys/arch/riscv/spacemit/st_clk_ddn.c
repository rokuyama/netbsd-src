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

#define	ST_CLK_DDN_RATE(ddn, div, mult, parent_rate)			\
    (((((parent_rate) / 10000) * (mult)) / ((ddn)->div_factor * (div))) * 10000)

static bool
st_clk_ddn_is_enabled(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;

	return (ST_CLK_RD(sc, clk, SEL) & ddn->gate_mask) != 0;
}

static int
st_clk_ddn_enable(struct st_clk_softc *sc, struct st_clk_clk *clk, bool enable)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;
	uint32_t reg;

	ST_CLK_LOCK(sc);

	reg = ST_CLK_RD(sc, clk, SEL);
	if (enable)
		reg |= ddn->gate_mask;
	else
		reg &= ~ddn->gate_mask;
	ST_CLK_WR(sc, clk, SEL, reg);

	ST_CLK_UNLOCK(sc);

	return 0;
}

static u_int
st_clk_ddn_get_rate(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;
	const u_int parent_rate = st_clk_get_parent_rate(clk);

	const uint32_t reg = ST_CLK_RD(sc, clk, CTL);
	const u_int div = __SHIFTOUT(reg, ddn->div_mask);
	const u_int mult = __SHIFTOUT(reg, ddn->mult_mask);

	return ST_CLK_DDN_RATE(ddn, div, mult, parent_rate);
}

static u_int
st_clk_ddn_round_rate(struct st_clk_softc *sc, struct st_clk_clk *clk,
    u_int target, u_int *dp, u_int *mp)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;
	const u_int parent_rate = st_clk_get_parent_rate(clk);
	u_int best_rate = 0;

	for (const struct st_clk_ddn_table *table = ddn->table;
	    table->div != 0; table++) {
		const u_int rate =
		    ST_CLK_DDN_RATE(ddn, table->div, table->mult, parent_rate);

		if (ST_CLK_UABS(rate, target) >= ST_CLK_UABS(best_rate, target))
			continue;

		best_rate = rate;
		if (dp != NULL)
			*dp = table->div;
		if (mp != NULL)
			*mp = table->mult;
	}

	return best_rate;
}

static int
st_clk_ddn_set_rate(struct st_clk_softc *sc, struct st_clk_clk *clk, u_int rate)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;
	uint32_t reg;
	u_int div, mult;
	int error = 0;

	ST_CLK_LOCK(sc);

	if (st_clk_ddn_round_rate(sc, clk, rate, &div, &mult) == 0) {
		error = EINVAL;
		goto out;
	}

	reg = ST_CLK_RD(sc, clk, CTL);
	reg &= ~(ddn->div_mask | ddn->mult_mask);
	reg |= __SHIFTIN(div, ddn->div_mask) | __SHIFTIN(mult, ddn->mult_mask);
	ST_CLK_WR(sc, clk, CTL, reg);

 out:
	ST_CLK_UNLOCK(sc);

	return error;
}

void
st_clk_ddn_init(struct st_clk_clk *clk)
{
	const struct st_clk_ddn *ddn = &clk->scc_ddn;
	struct st_clk_funcs * const fp = &clk->scc_funcs;

	DPRINTF(VERBOSE, "%u: %s\n", clk->scc_id, clk->scc_name);

	KASSERT(ddn->div_mask != 0);
	KASSERT(ddn->mult_mask != 0);
	KASSERT(ddn->div_factor != 0);
	KASSERT(ddn->table != NULL);

	if (ddn->gate_mask != 0) {
		fp->is_enabled = st_clk_ddn_is_enabled;
		fp->enable = st_clk_ddn_enable;
	}

	fp->get_rate = st_clk_ddn_get_rate;
	fp->round_rate = st_clk_ddn_round_rate;
	fp->set_rate = st_clk_ddn_set_rate;
}
