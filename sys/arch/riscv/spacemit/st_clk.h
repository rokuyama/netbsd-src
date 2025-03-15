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

#ifndef __RISCV_SPACEMIT_CLK_H
#define	__RISCV_SPACEMIT_CLK_H

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_backend.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_cru.h>

enum st_clk_type {
	ST_CLK_TYPE_PLL,
	ST_CLK_TYPE_MIX,
	ST_CLK_TYPE_DDN,
};

enum st_clk_reg {
	ST_CLK_REG_CTL,		// PMD
	ST_CLK_REG_SEL,		// pMD
	ST_CLK_REG_XTC,		// P..
	ST_CLK_NREGS,
};

#define	ST_CLK_XTC_DIV		__BITS(0, 30)
#define	ST_CLK_XTC_ENABLED	__BIT(31)

struct st_clk_clk;
struct st_clk_softc;
struct st_clk_pll_table;

struct st_clk_funcs {
	bool		(*is_enabled)(struct st_clk_softc *,
			    struct st_clk_clk *);
	int		(*enable)(struct st_clk_softc *, struct st_clk_clk *,
			    bool);
	const char *	(*get_parent)(struct st_clk_softc *,
			    struct st_clk_clk *);
	int		(*set_parent)(struct st_clk_softc *,
			    struct st_clk_clk *, const char *);
	u_int		(*get_rate)(struct st_clk_softc *, struct st_clk_clk *);
	u_int		(*round_rate)(struct st_clk_softc *,
			    struct st_clk_clk *, u_int, u_int *, u_int *);
	int		(*set_rate)(struct st_clk_softc *, struct st_clk_clk *,
			    u_int);
};

struct st_clk_pll {
	u_int				lock_handle;
	bus_size_t			lock_reg;
	uint32_t			lock_mask;
	const struct st_clk_pll_table *	table;
};

struct st_clk_mix {
	uint32_t			div_mask;
	uint32_t			fc_mask;
	uint32_t			pid_mask;
	uint32_t			gate_mask;
	uint32_t			gate_enable;
	uint32_t			gate_disable;
	u_int				factor_div;
	u_int				factor_mult;
};

struct st_clk_ddn {
	uint32_t			gate_mask;
	uint32_t			div_mask;
	uint32_t			mult_mask;
	u_int				div_factor;
	const struct st_clk_ddn_table *	table;
};

union st_clk_union {
	struct st_clk_pll		u_pll;
	struct st_clk_mix		u_mix;
	struct st_clk_ddn		u_ddn;
};

struct st_clk_clk {
	struct clk			scc_base;
	u_int				scc_id;
	enum st_clk_type		scc_type;
	struct st_clk_funcs		scc_funcs;
	const char **			scc_parents;
	u_int				scc_nparents;
	u_int				scc_handle;
	bus_size_t			scc_regs[ST_CLK_NREGS];
	union st_clk_union		scc_u;
	uint32_t			scc_quirk_flags;
	uint32_t			scc_saved_reg;
};
#define	scc_domain	scc_base.domain
#define	scc_name	scc_base.name
#define	scc_flags	scc_base.flags
#define	scc_pll		scc_u.u_pll
#define	scc_mix		scc_u.u_mix
#define	scc_ddn		scc_u.u_ddn

#define	ST_CLK_PLL_ENTRY(id, name, handle, ctl, sel, xtc,		\
    _lock_handle, _lock_reg, _lock_mask, _table) {			\
	.scc_id = id,							\
	.scc_name = #name,						\
	.scc_type = ST_CLK_TYPE_PLL,					\
	.scc_parents = ST_CLK_SINGLE_PARENTS(NULL),			\
	.scc_nparents = 0,						\
	.scc_handle = handle,						\
	.scc_regs[ST_CLK_REG_CTL] = ctl,				\
	.scc_regs[ST_CLK_REG_SEL] = sel,				\
	.scc_regs[ST_CLK_REG_XTC] = xtc,				\
	.scc_pll = {							\
		.lock_handle = _lock_handle,				\
		.lock_reg = _lock_reg,					\
		.lock_mask = _lock_mask,				\
		.table = _table,					\
	},								\
    }

#define	ST_CLK_MIX_ENTRY(id, name, parents, nparents, handle, ctl, sel,	\
    _div_mask, _fc_mask, _pid_mask, _gate_mask, _factor_div, _factor_mult) { \
	.scc_id = id,							\
	.scc_name = #name,						\
	.scc_type = ST_CLK_TYPE_MIX,					\
	.scc_parents = parents,						\
	.scc_nparents = nparents,					\
	.scc_handle = handle,						\
	.scc_regs[ST_CLK_REG_CTL] = ctl,				\
	.scc_regs[ST_CLK_REG_SEL] = sel,				\
	.scc_mix = {							\
		.div_mask = _div_mask,					\
		.fc_mask = _fc_mask,					\
		.pid_mask = _pid_mask,					\
		.gate_mask = _gate_mask,				\
		.gate_enable = _gate_mask,				\
		.factor_div = _factor_div,				\
		.factor_mult = _factor_mult,				\
	},								\
    }

#define	ST_CLK_DDN_ENTRY(id, name, parent, handle, ctl, sel,		\
    _gate_mask, _div_mask, _mult_mask, _div_factor, _table) {		\
	.scc_id = id,							\
	.scc_name = #name,						\
	.scc_type = ST_CLK_TYPE_DDN,					\
	.scc_parents = ST_CLK_SINGLE_PARENTS(parent),			\
	.scc_nparents = 1,						\
	.scc_handle = handle,						\
	.scc_regs[ST_CLK_REG_CTL] = ctl,				\
	.scc_regs[ST_CLK_REG_SEL] = sel,				\
	.scc_ddn = {							\
		.gate_mask = _gate_mask,				\
		.div_mask = _div_mask,					\
		.mult_mask = _mult_mask,				\
		.div_factor = _div_factor,				\
		.table = _table,					\
	},								\
    }

#define	ST_CLK_SINGLE_PARENTS(parent)	(const char *[]) { parent, }

#define	ST_CLK_QUIRK_MIX_GATE_DELAY		__BIT(0)
#define	ST_CLK_QUIRK_MIX_SEL_WRONLY		__BIT(1)

#define	ST_CLK_QUIRK(clk, quirk)					\
    (((clk)->scc_quirk_flags & ST_CLK_QUIRK_ ## quirk) != 0)

struct st_clk_quirk {
	const char *			name;
	int				(*func)(struct st_clk_clk *);
};

struct st_clk_softc {
	device_t			sc_dev;
	int				sc_phandle;
	bus_space_tag_t			sc_bst;
	u_int				sc_nhandles;
	struct clk_domain		sc_domain;
	struct st_clk_clk *		sc_clks;
	u_int				sc_nclks;
	const struct st_clk_quirk *	sc_quirks;
	const char **			sc_boot_enable;
};

#define	ST_CLK_REG(clk, name)						\
    ((clk)->scc_regs[ST_CLK_REG_ ## name])

#define	ST_CLK_RAW_RD(sc, handle, offset)				\
    st_cru_read(handle, offset)

#define	ST_CLK_RAW_WR(sc, handle, offset, val)				\
    st_cru_write(handle, offset, val)

#define	ST_CLK_RD(sc, clk, regname)					\
    st_cru_read((clk)->scc_handle, ST_CLK_REG(clk, regname))

#define	ST_CLK_WR(sc, clk, regname, regval)				\
    st_cru_write((clk)->scc_handle, ST_CLK_REG(clk, regname), regval)

#define	ST_CLK_LOCK(sc)		st_cru_lock()
#define	ST_CLK_UNLOCK(sc)	st_cru_unlock()
#define	ST_CLK_LOCKED(sc)	st_cru_locked()

// XXX Shouldn't be here...
#define	ST_CLK_UABS(x, y)	((x) > (y) ? (x) - (y) : (y) - (x))

struct st_clk_pll_table {
	u_int				rate;
	uint32_t			ctl;
	uint32_t			div;
};

#define	ST_CLK_PLL_TABLE_ENTRY(_rate, _ctl, _div)			\
    { .rate = _rate, .ctl = _ctl, .div = _div, }

#define	ST_CLK_PLL_TABLE_EOL	ST_CLK_PLL_TABLE_ENTRY(0, 0, 0)

struct st_clk_ddn_table {
	u_int				div;
	u_int				mult;
};

#define	ST_CLK_DDN_TABLE_ENTRY(_div, _mult)				\
    { .div = _div, .mult = _mult, }

#define	ST_CLK_DDN_TABLE_EOL	ST_CLK_DDN_TABLE_ENTRY(0, 0)

#define	ST_CLK_SINGLE_DDN_TABLE(div, mult)				\
    (const struct st_clk_ddn_table []) {				\
	ST_CLK_DDN_TABLE_ENTRY(div, mult),				\
	ST_CLK_DDN_TABLE_EOL,						\
    }

#define	ST_CLK_GET_PARENT(clk)						\
    (struct st_clk_clk *)clk_get_parent(&(clk)->scc_base)

#define	ST_CLK_GET_RATE(clk)		clk_get_rate(&(clk)->scc_base)
#define	ST_CLK_ENABLE(clk)		clk_enable(&(clk)->scc_base)
#define	ST_CLK_DISABLE(clk)		clk_disable(&(clk)->scc_base)

static inline u_int
st_clk_get_parent_rate(struct st_clk_clk *clk)
{
	struct clk * const parent_base = clk_get_parent(&clk->scc_base);

//	KASSERT(parent_base != NULL);
	return parent_base == NULL ? 0 : clk_get_rate(parent_base);
}

#define	ST_CLK_FDTBUS_BYNAME(name)					\
    (struct st_clk_clk *)fdtbus_clock_byname(name)

void	st_clk_init(struct st_clk_softc *);
bool	st_clk_is_enabled(struct st_clk_softc *, struct st_clk_clk *);
u_int	st_clk_round_rate(struct st_clk_softc *, struct st_clk_clk *, u_int,
	    u_int *, u_int *);

void	st_clk_pll_init(struct st_clk_clk *);
void	st_clk_mix_init(struct st_clk_clk *);
void	st_clk_ddn_init(struct st_clk_clk *);

extern int st_clk_debug;

#define	ST_CLK_DEBUG_CLK	__BIT(0)
#define	ST_CLK_DEBUG_FDT	__BIT(1)
#define	ST_CLK_DEBUG_DUMP	__BIT(2)
#define	ST_CLK_DEBUG_VERBOSE	__BIT(3)

#if 1
#define	DPRINTF(flag, fmt, args...)					\
    if ((st_clk_debug & ST_CLK_DEBUG_ ## flag) != 0) {			\
	printf("%s: %d: " fmt, __func__, __LINE__, ##args);		\
    }
#else
#define	DPRINTF(fmt, args...)	__nothing
#endif

#endif /* !__RISCV_SPACEMIT_CLK_H */
