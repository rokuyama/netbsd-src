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

#define	ST_CLK_MIX_FACTOR_RATE(mix, parent_rate)			\
   (((parent_rate) * (mix)->factor_mult) / (mix)->factor_div)

static uint32_t
st_clk_mix_sel_rd(struct st_clk_softc *sc, struct st_clk_clk *clk)
{

	if (ST_CLK_QUIRK(clk, MIX_SEL_WRONLY))
		return clk->scc_saved_reg;

	return ST_CLK_RD(sc, clk, SEL);
}

static void
st_clk_mix_sel_wr(struct st_clk_softc *sc, struct st_clk_clk *clk, uint32_t val)
{

	if (ST_CLK_QUIRK(clk, MIX_SEL_WRONLY))
		clk->scc_saved_reg = val;

	ST_CLK_WR(sc, clk, SEL, val);
}

static int
st_clk_mix_fc_set(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	uint32_t reg;

	ST_CLK_LOCKED(sc);

	if (mix->fc_mask == 0)
		return 0;

	reg = ST_CLK_RD(sc, clk, CTL);
	reg |= mix->fc_mask;
	ST_CLK_WR(sc, clk, CTL, reg);

	for (u_int retry = 5000; retry > 0; retry--) {
		reg = ST_CLK_RD(sc, clk, CTL);
		if ((reg & mix->fc_mask) != 0)
			return 0;
	}

	return ETIMEDOUT;
}

static bool
st_clk_mix_is_enabled(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	const uint32_t reg = st_clk_mix_sel_rd(sc, clk);

	return (reg & mix->gate_mask) == mix->gate_enable;
}

static int
st_clk_mix_enable(struct st_clk_softc *sc, struct st_clk_clk *clk, bool enable)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	uint32_t reg;
	int error = 0;

	ST_CLK_LOCK(sc);

	reg = st_clk_mix_sel_rd(sc, clk);
	reg &= ~mix->gate_mask;
	if (enable)
		reg |= mix->gate_enable;
	else
		reg |= mix->gate_disable;
	st_clk_mix_sel_wr(sc, clk, reg);

	if (enable) {
		u_int usec = 1;
 again:
		if (!st_clk_is_enabled(sc, clk)) {
			if (usec > 10 * 1000) {
				error = ETIMEDOUT;
				goto out;
			}
			delay(usec);
			usec *= 10;
			goto again;
		}
	}

	if (ST_CLK_QUIRK(clk, MIX_GATE_DELAY)) {
		const u_int rate = ST_CLK_GET_RATE(clk);

		KASSERT(rate != 0);
		delay(howmany(2 * 1000 * 1000, rate));
	}

 out:
	ST_CLK_UNLOCK(sc);

	return error;
}

static const char *
st_clk_mix_get_parent(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	const u_int pid =
	    __SHIFTOUT(st_clk_mix_sel_rd(sc, clk), mix->pid_mask);

	KASSERT(pid < clk->scc_nparents);
	return clk->scc_parents[pid];
}

static int
st_clk_mix_set_parent(struct st_clk_softc *sc, struct st_clk_clk *clk,
    const char *parent)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	uint32_t reg;
	u_int pid;

	for (pid = 0; pid < clk->scc_nparents; pid++) {
		if (strcmp(parent, clk->scc_parents[pid]) == 0)
			goto found;
	}
	return EINVAL;

 found:
	ST_CLK_LOCK(sc);

	reg = st_clk_mix_sel_rd(sc, clk);
	reg &= ~mix->pid_mask;
	reg |= __SHIFTIN(pid, mix->pid_mask);
	st_clk_mix_sel_wr(sc, clk, reg);

	const int error = st_clk_mix_fc_set(sc, clk);

	ST_CLK_UNLOCK(sc);

	return error;
}

static u_int
st_clk_mix_get_rate(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	const u_int parent_rate = st_clk_get_parent_rate(clk);

	if (mix->div_mask == 0)
		return ST_CLK_MIX_FACTOR_RATE(mix, parent_rate);

#if 0
	const u_int div = __SHIFTOUT(st_clk_mix_sel_rd(sc, clk), mix->div_mask);
	//KASSERT(div > 0);
	return div == 0 ? 0 : howmany(parent_rate, div);
#endif

	const u_int div =
	    __SHIFTOUT(st_clk_mix_sel_rd(sc, clk), mix->div_mask) + 1;
	return howmany(parent_rate, div);
}

static u_int
st_clk_mix_round_rate(struct st_clk_softc *sc, struct st_clk_clk *clk,
    u_int target, u_int *pp, u_int *dp)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	u_int best_rate = 0;

	for (u_int pid = 0; pid < clk->scc_nparents; pid++) {
		const u_int parent_rate = st_clk_get_parent_rate(clk);

		if (mix->div_mask == 0) {
			const u_int rate =
			    ST_CLK_MIX_FACTOR_RATE(mix, parent_rate);

			if (ST_CLK_UABS(rate, target) >=
			    ST_CLK_UABS(best_rate, target))
				continue;

			best_rate = rate;
			if (pp != NULL)
				*pp = pid;
			if (dp != NULL)
				*dp = 0;

			continue;
		}

		for (u_int div = 1; div <= __SHIFTOUT_MASK(mix->div_mask);
		    div++) {
			const u_int rate = howmany(parent_rate, div);

			if (ST_CLK_UABS(rate, target) >=
			    ST_CLK_UABS(best_rate, target))
				continue;

			best_rate = rate;
			if (pp != NULL)
				*pp = pid;
			if (dp != NULL)
				*dp = div;
		}
	}

	return best_rate;
}

static int
st_clk_mix_set_rate(struct st_clk_softc *sc, struct st_clk_clk *clk, u_int rate)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	uint32_t reg;
	u_int pid, div;
	int error = 0;

	ST_CLK_LOCK(sc);

	if (st_clk_mix_round_rate(sc, clk, rate, &pid, &div) == 0) {
		error = EINVAL;
		goto out;
	}

	reg = st_clk_mix_sel_rd(sc, clk);
	if (mix->pid_mask != 0) {
		KASSERT(pid < clk->scc_nparents);
		reg &= ~mix->pid_mask;
		reg |= __SHIFTIN(pid, mix->pid_mask);
	} else {
		KASSERT(pid == 0);
	}
	if (mix->div_mask != 0) {
		KASSERT(div > 0);
		reg &= ~mix->div_mask;
#if 0
		reg |= __SHIFTIN(div, mix->div_mask);
#else
		reg |= __SHIFTIN(div - 1, mix->div_mask);
#endif
	} else {
		KASSERT(div == 0);
	}
	st_clk_mix_sel_wr(sc, clk, reg);

	// XXXRO Is this OK?
	error = (mix->pid_mask != 0) ? st_clk_mix_fc_set(sc, clk) : 0;

 out:
	ST_CLK_UNLOCK(sc);

	return error;
}

void
st_clk_mix_init(struct st_clk_clk *clk)
{
	const struct st_clk_mix *mix = &clk->scc_mix;
	struct st_clk_funcs *fp = &clk->scc_funcs;

	DPRINTF(VERBOSE, "%u: %s\n", clk->scc_id, clk->scc_name);

	if (mix->gate_mask != 0) {
		KASSERT(mix->gate_enable != 0 || mix->gate_disable != 0);
		fp->is_enabled = st_clk_mix_is_enabled;
		fp->enable = st_clk_mix_enable;
	}

	if (mix->pid_mask != 0) {
		KASSERT(clk->scc_nparents > 1);
		KASSERT(clk->scc_nparents <=
		    __SHIFTOUT_MASK(mix->pid_mask) + 1);
		fp->get_parent = st_clk_mix_get_parent;
		fp->set_parent = st_clk_mix_set_parent;
	} else {
		KASSERT(clk->scc_nparents <= 1);
	}

	fp->get_rate = st_clk_mix_get_rate;

	if (mix->div_mask != 0) {
		KASSERT(mix->factor_div == 0);
		KASSERT(mix->factor_mult == 0);
		fp->round_rate = st_clk_mix_round_rate;
		fp->set_rate = st_clk_mix_set_rate;
	} else {
		KASSERT(mix->factor_div != 0);
		KASSERT(mix->factor_mult != 0);
	}
}
