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
#include <sys/kmem.h>
#include <sys/systm.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_backend.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_clk.h>

#if 1
int st_clk_debug = ST_CLK_DEBUG_CLK | ST_CLK_DEBUG_FDT;
#else
int st_clk_debug = ST_CLK_DEBUG_DUMP;
#endif

static struct st_clk_clk *
st_clk_find(struct st_clk_softc *sc, const char *name)
{

	for (u_int id = 0; id < sc->sc_nclks; id++) {
		struct st_clk_clk * const clk = &sc->sc_clks[id];

		if (strcmp(name, clk->scc_name) == 0)
			return clk;
	}

	return NULL;
}

static void
st_clk_dump(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const char *type_names[] = {
		[ST_CLK_TYPE_PLL] = "PLL",
		[ST_CLK_TYPE_MIX] = "MIX",
		[ST_CLK_TYPE_DDN] = "DDN",
	};
	u_int rate, ghz, mhz, khz;

	rate = clk_get_rate(&clk->scc_base);
	ghz = rate / (1000 * 1000 * 1000);
	rate -= ghz * (1000 * 1000 * 1000);
	mhz = rate / (1000 * 1000);
	rate -= mhz * (1000 * 1000);
	khz = rate / 1000;
	rate -= khz * 1000;

	printf("%3u %-4s %-28s ", clk->scc_id, type_names[clk->scc_type],
	    clk->scc_name);

	if (ghz > 0)
		printf("%1u,%03u,%03u,%03u Hz\n", ghz, mhz, khz, rate);
	else if (mhz > 0)
		printf("  %3u,%03u,%03u Hz\n", mhz, khz, rate);
	else if (khz > 0)
		printf("      %3u,%03u Hz\n", khz, rate);
	else
		printf("          %3u Hz\n", rate);

	struct clk * const parent_base = clk_get_parent(&clk->scc_base);

	if (parent_base == NULL)
		return;

	struct st_clk_clk * const parent = st_clk_find(sc, parent_base->name);

	if (parent != NULL) {
		printf("<- %s [%u:%s]\n", parent->scc_name, parent->scc_id,
		    type_names[parent->scc_type]);
	} else {
		printf("<- %s [%s]\n", parent_base->name,
		    parent_base->domain->name);
	}
}

static void
st_clk_dump_all(struct st_clk_softc *sc)
{

	if ((st_clk_debug & ST_CLK_DEBUG_DUMP) == 0)
		return;

	printf("%s: %u clocks\n", sc->sc_domain.name, sc->sc_nclks);

	for (u_int id = 0; id < sc->sc_nclks; id++) {
		struct st_clk_clk * const clk = &sc->sc_clks[id];

		st_clk_dump(sc, clk);
	}
}

static struct clk *
st_clk_fdt_clock_decode(device_t dev, int phandle, const void *data, size_t len)
{
	struct st_clk_softc * const sc = device_private(dev);

	if (data == NULL) {
		DPRINTF(FDT, "NULL input\n");
		return NULL;
	}

	if (len != 4) {
		DPRINTF(FDT, "invalid len %zu\n", len);
		return NULL;
	}

	const uint32_t id = be32dec(data);

	if (id < sc->sc_nclks) {
		struct st_clk_clk * const clk = &sc->sc_clks[id];

		DPRINTF(FDT, "0x%x: %s: %u\n", id, clk->scc_name, clk->scc_id);
		return &clk->scc_base;
	}

	DPRINTF(FDT, "0x%x: not found\n", id);
	return NULL;
}

static const struct fdtbus_clock_controller_func st_clk_fdt_clock_funcs = {
	.decode = st_clk_fdt_clock_decode,
};

static struct clk *
st_clk_clock_get(void *priv, const char *name)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = st_clk_find(sc, name);

	if (clk == NULL) {
		printf("%s: %s: not found\n", __func__, name);
		return NULL;
	}

	DPRINTF(CLK, "%s: %u\n", clk->scc_name, clk->scc_id);
	return &clk->scc_base;
}

static void
st_clk_clock_put(void *priv, struct clk *clk)
{
}

static u_int
st_clk_clock_get_rate(void *priv, struct clk *base)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	u_int rate;

	if (fp->get_rate != NULL) {
		rate = fp->get_rate(sc, clk);
		DPRINTF(CLK, "%s: %u Hz\n", clk->scc_name, rate);
		return rate;
	}

	return st_clk_get_parent_rate(clk);
}

static int
st_clk_clock_set_rate(void *priv, struct clk *base, u_int rate)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;

#if 0 // notyet
	if ((clk->scc_flags & CLK_SET_RATE_PARENT) != 0) {
		struct clk * const parent_base = clk_get_parent(base);
		if (parent_base == NULL) {
			printf("%s: %s: no parent\n", __func__, name);
			return ENXIO;
		}
		DPRINTF(CLK, "%s: parent: %s: %u Hz\n",
		    clk->scc_name, parent_base->name, rate);
		return clk_set_rate(parent_base, rate);
	}
#endif

	if (fp->set_rate != NULL) {
		DPRINTF(CLK, "%s: %u Hz\n", clk->scc_name, rate);
		return fp->set_rate(sc, clk, rate);
	}

	printf("%s: %s: unable to set rate\n", __func__, clk->scc_name);
	return ENXIO;
}

static u_int
st_clk_clock_round_rate(void *priv, struct clk *base, u_int rate)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	u_int rounded;

#if 0 // notyet
	if ((clk->scc_flags & CLK_SET_RATE_PARENT) != 0) {
		struct clk * const parent_base = clk_get_parent(base);
		if (parent == NULL) {
			printf("%s: %s: no parent\n", __func__, clk->scc_name);
			return 0;
		}
		rounded = clk_round_rate(parent_base, rate);
		DPRINTF(CLK, "%s: parent: %s: %u Hz --> %u Hz\n",
		    name, parent->name, rate, rounded);
		return rounded;
	}
#endif

	if (fp->round_rate != NULL) {
		rounded = fp->round_rate(sc, clk, rate, NULL, NULL);
		DPRINTF(CLK, "%s: %u Hz --> %u Hz\n",
		    clk->scc_name, rate, rounded);
		return rounded;
	}

	printf("%s: %s: unable to round rate %u\n",
	    __func__, clk->scc_name, rate);
	return 0;
}

static int
st_clk_clock_enable(void *priv, struct clk *base)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	int error = 0;

	struct clk * const parent_base = clk_get_parent(base);
	if (parent_base != NULL) {
		error = clk_enable(parent_base);
		DPRINTF(CLK, "%s: parent: %s: %d\n",
		    clk->scc_name, parent_base->name, error);
		if (error != 0)
			return error;
	}

	if (fp->enable != NULL) {
		if (fp->is_enabled == NULL || !fp->is_enabled(sc, clk))
			error = fp->enable(sc, clk, true);
	}

	DPRINTF(CLK, "%s: %d\n", clk->scc_name, error);
	return error;
}

static int
st_clk_clock_disable(void *priv, struct clk *base)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	int error = EINVAL;

	if (fp->enable != NULL) {
		if (fp->is_enabled == NULL || fp->is_enabled(sc, clk))
			error = fp->enable(sc, clk, false);
	}

#if 0 // notyet
	if ((clk->scc_flags & CLK_FLAG_CRITICAL) != 0) {
		device_printf(sc->sc_dev, "critical clock %s disabled\n",
		    clk->scc_name);
	}
#endif

	DPRINTF(CLK, "%s: %d\n", clk->scc_name, error);
	return error;
}

static int
st_clk_clock_set_parent(void *priv, struct clk *base, struct clk *parent_base)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	int error = EINVAL;

	if (fp->set_parent != NULL)
		error = fp->set_parent(sc, clk, parent_base->name);

	DPRINTF(CLK, "%s: %s: %d\n", clk->scc_name, parent_base->name, error);
	return error;
}

static struct clk *
st_clk_clock_get_parent(void *priv, struct clk *base)
{
	struct st_clk_softc * const sc = priv;
	struct st_clk_clk * const clk = (struct st_clk_clk *)base;
	const struct st_clk_funcs *fp = &clk->scc_funcs;
	const char *parent_name;

	if (fp->get_parent != NULL) {
		KASSERT(clk->scc_nparents > 1);
		parent_name = fp->get_parent(sc, clk);
	} else {
		KASSERT(clk->scc_nparents <= 1);
		parent_name = clk->scc_parents[0];
	}

	if (parent_name == NULL) {
		DPRINTF(VERBOSE, "%s: parent not found\n", clk->scc_name);
		return NULL;
	}

	struct st_clk_clk * const parent = st_clk_find(sc, parent_name);
	if (parent != NULL) {
		DPRINTF(VERBOSE, "%s: parent: %s\n",
		    clk->scc_name, parent_name);
		return &parent->scc_base;
	}

	/* No parent in this domain, try FDT */
	struct clk * const parent_base = fdtbus_clock_byname(parent_name);
	if (parent_base == NULL) {
		DPRINTF(VERBOSE, "%s: parent (%s) not found\n", clk->scc_name,
		    parent_name);
		return NULL;
	}
	DPRINTF(VERBOSE, "%s: parent: %s: domain: %s\n",
	    clk->scc_name, parent_name, parent_base->domain->name);
	return parent_base;
}

static const struct clk_funcs st_clk_clock_funcs = {
	.get = st_clk_clock_get,
	.put = st_clk_clock_put,
	.get_rate = st_clk_clock_get_rate,
	.set_rate = st_clk_clock_set_rate,
	.round_rate = st_clk_clock_round_rate,
	.enable = st_clk_clock_enable,
	.disable = st_clk_clock_disable,
	.set_parent = st_clk_clock_set_parent,
	.get_parent = st_clk_clock_get_parent,
};

bool
st_clk_is_enabled(struct st_clk_softc *sc, struct st_clk_clk *clk)
{
	const struct st_clk_funcs *fp = &clk->scc_funcs;

	if (fp->is_enabled != NULL)
		return fp->is_enabled(sc, clk);

	// XXXRO
	return true;
}

u_int
st_clk_round_rate(struct st_clk_softc *sc, struct st_clk_clk *clk,
    u_int rate, u_int *p0, u_int *p1)
{
	const struct st_clk_funcs *fp = &clk->scc_funcs;

	if (fp->round_rate != NULL)
		return fp->round_rate(sc, clk, rate, p0, p1);

	return 0;
}

void
st_clk_init(struct st_clk_softc *sc)
{
	device_t dev = sc->sc_dev;
	const int phandle = sc->sc_phandle;

	sc->sc_bshs =
	    kmem_alloc(sc->sc_nbshs * sizeof(sc->sc_bshs[0]), KM_SLEEP);
	for (u_int i = 0; i < sc->sc_nbshs; i++) {
		bus_addr_t addr;
		bus_size_t size;

		if (fdtbus_get_reg(phandle, i, &addr, &size) != 0) {
			aprint_error(": couldn't get registers: %d\n", i);
			return;
		}
		if (bus_space_map(sc->sc_bst, addr, size, 0,
		    &sc->sc_bshs[i]) != 0) {
			aprint_error(": couldn't map registers: %d: "
			"0x%" PRIxBUSADDR ": 0x%" PRIxBUSSIZE "\n",
			i, addr, size);
			return;
		}
	}

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);

	aprint_naive("\n");
	aprint_normal(": SpacemiT clock controller\n");

	for (const struct st_clk_quirk *q = sc->sc_quirks;
	    q->name != NULL; q++) {
		struct st_clk_clk * const clk = st_clk_find(sc, q->name);

		if (clk == NULL) {
			aprint_error_dev(dev, "%s in quirk table not found\n",
			    q->name);
			continue;
		}

		if (q->func(clk) != 0) {
			aprint_error_dev(dev, "%s: qurik coulnd't apply\n",
			    q->name);
		}
	}

	for (u_int id = 0; id < sc->sc_nclks; id++) {
		struct st_clk_clk * const clk = &sc->sc_clks[id];

		KASSERTMSG(clk->scc_name != NULL, "clk %u: not configured\n",
		    id);

		KASSERTMSG(clk->scc_id == id, "clk %u (%s): id %u corrupted\n",
		    id, clk->scc_name, clk->scc_id);

		switch (clk->scc_type) {
		case ST_CLK_TYPE_PLL:
			st_clk_pll_init(clk);
			break;
		case ST_CLK_TYPE_MIX:
			st_clk_mix_init(clk);
			break;
		case ST_CLK_TYPE_DDN:
			st_clk_ddn_init(clk);
			break;
		default:
			panic("clk %u (%s): invalid type %d\n",
			    id, clk->scc_name, clk->scc_type);
		}
	}

	sc->sc_domain.name = device_xname(sc->sc_dev);
	sc->sc_domain.funcs = &st_clk_clock_funcs;
	sc->sc_domain.priv = sc;

	for (u_int id = 0; id < sc->sc_nclks; id++) {
		struct st_clk_clk * const clk = &sc->sc_clks[id];

		clk->scc_domain = &sc->sc_domain;
		clk_attach(&clk->scc_base);
	}

	for (const char **p = sc->sc_boot_enable; *p != NULL; p++) {
		struct st_clk_clk * const clk = st_clk_find(sc, *p);

		if (clk == NULL) {
			aprint_error_dev(dev, "%s marked boot-enable but "
			    "not found\n", *p);
			continue;
		}

		if (clk_enable(&clk->scc_base) != 0) {
			aprint_error_dev(dev, "%s: couldn't enable\n", *p);
		}
	}

	fdtbus_register_clock_controller(dev, phandle, &st_clk_fdt_clock_funcs);

	st_clk_dump_all(sc);
}
