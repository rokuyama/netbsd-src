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

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_cru.h>
#include <riscv/spacemit/st_reset.h>

#define	DPRINTF(fmt, args...)						\
    printf("%s: %d: " fmt, __func__, __LINE__, ##args)

static void *	st_reset_acquire(device_t, const void *, size_t);
static void	st_reset_release(device_t, void *);
static int	st_reset_assert(device_t, void *);
static int	st_reset_deassert(device_t, void *);

static const struct fdtbus_reset_controller_func st_reset_funcs = {
	.acquire = st_reset_acquire,
	.release = st_reset_release,
	.reset_assert = st_reset_assert,
	.reset_deassert = st_reset_deassert,
};

void
st_reset_init(struct st_reset_softc *sc)
{

	if (st_cru_init(sc->sc_bst, sc->sc_phandle, sc->sc_nhandles) != 0)
		panic("st_cru_init");

	aprint_naive("\n");
	aprint_normal(": SpacemiT reset controller\n");

	fdtbus_register_reset_controller(sc->sc_dev, sc->sc_phandle,
	    &st_reset_funcs);
}

/*
 * Interface as fdt(4) reset controller
 */

#define	ST_RESET_TARGET2PRIV(target)					\
    ((void *)(uintptr_t)(__BIT(31) | (target)))
#define	ST_RESET_PRIV2TARGET(priv)					\
    (KASSERT(((uintptr_t)(priv) & __BIT(31)) != 0),			\
	(uintptr_t)(priv) & ~__BIT(31))

static void *
st_reset_acquire(device_t dev, const void *data, size_t len)
{
	struct st_reset_softc * const sc = device_private(dev);

	if (len != 4) {
		DPRINTF("invalid len %zu\n", len);
		return NULL;
	}

	const u_int target = be32dec(data);
	if (target >= sc->sc_nresets) {
		DPRINTF("invalid target %u\n", target);
		return NULL;
	}

	const struct st_reset * const reset = &sc->sc_resets[target];
	if (reset->assert_mask == 0 && reset->deassert_mask == 0) {
		DPRINTF("not-initialized target %u\n", target);
		return NULL;
	}

	DPRINTF("target %u\n", target);

	return ST_RESET_TARGET2PRIV(target);
}

static void
st_reset_release(device_t dev, void *priv)
{
	const u_int target = ST_RESET_PRIV2TARGET(priv);

	DPRINTF("target %u\n", target);
	__USE(target);
}

static int
st_reset_assert(device_t dev, void *priv)
{
	struct st_reset_softc * const sc = device_private(dev);
	const u_int target = ST_RESET_PRIV2TARGET(priv);

	KASSERT(target < sc->sc_nresets);

	const struct st_reset * const reset = &sc->sc_resets[target];

	DPRINTF("target %u: assert 0x%08x, deassert 0x%08x\n",
	    target, reset->assert_mask, reset->deassert_mask);

	st_cru_lock();

	uint32_t val = st_cru_read(reset->handle, reset->offset);
	DPRINTF("target %u: initial 0x%08x\n", target, val);
	val &= ~(reset->assert_mask | reset->deassert_mask);
	val |= reset->assert_mask;
	DPRINTF("target %u: prepare 0x%08x\n", target, val);
	st_cru_write(reset->handle, reset->offset, val);
	DPRINTF("target %u: confirm 0x%08x\n",
	    target, st_cru_read(reset->handle, reset->offset));

	st_cru_unlock();

	return 0;
}

static int
st_reset_deassert(device_t dev, void *priv)
{
	struct st_reset_softc * const sc = device_private(dev);
	const u_int target = ST_RESET_PRIV2TARGET(priv);

	KASSERT(target < sc->sc_nresets);

	const struct st_reset * const reset = &sc->sc_resets[target];

	DPRINTF("target %u: assert 0x%08x, deassert 0x%08x\n",
	    target, reset->assert_mask, reset->deassert_mask);

	st_cru_lock();

	uint32_t val = st_cru_read(reset->handle, reset->offset);
	DPRINTF("target %u: initial 0x%08x\n", target, val);
	val &= ~(reset->assert_mask | reset->deassert_mask);
	val |= reset->deassert_mask;
	DPRINTF("target %u: prepare 0x%08x\n", target, val);
	st_cru_write(reset->handle, reset->offset, val);
	DPRINTF("target %u: confirm 0x%08x\n",
	    target, st_cru_read(reset->handle, reset->offset));

	st_cru_unlock();

	return 0;
}
