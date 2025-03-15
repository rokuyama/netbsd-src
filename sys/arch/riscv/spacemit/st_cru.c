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
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_cru.h>

struct st_cru_handle {
	bus_addr_t		addr;
	bus_size_t		size;
	bus_space_handle_t	bsh;
};

struct st_cru {
	bus_space_tag_t		bst;
	u_int			nhandles;
	struct st_cru_handle *	handles;
	kmutex_t		lock;
};

static struct st_cru *cru;

int
st_cru_init(bus_space_tag_t bst, int phandle, u_int nhandles)
{
	int error;

	if (cru != NULL) {
		if (!bus_space_is_equal(bst, cru->bst)) {
			aprint_error(": bus mismatched\n");
			return EINVAL;
		}
		if (nhandles != cru->nhandles) {
			aprint_error(
			    ": number of register regions mismatched\n");
			return EINVAL;
		}
		for (u_int i = 0; i < nhandles; i++) {
			struct st_cru_handle * const hp = &cru->handles[i];
			bus_addr_t addr;
			bus_size_t size;

			error = fdtbus_get_reg(phandle, i, &addr, &size);
			if (error != 0) {
				aprint_error(
				    ": couldn't get registers: %u\n", i);
				return error;
			}
			if (addr != hp->addr || size != hp->size) {
				aprint_error(
				    ": register region mismatched: %u\n", i);
				return EINVAL;
			}
		}
		return 0;
	}

	ASSERT_SLEEPABLE();
	cru = kmem_zalloc(sizeof(*cru), KM_SLEEP);
	cru->bst = bst;
	cru->nhandles = nhandles;
	cru->handles =
	    kmem_zalloc(nhandles * sizeof(cru->handles[0]), KM_SLEEP);

	for (u_int i = 0; i < nhandles; i++) {
		struct st_cru_handle * const hp = &cru->handles[i];

		error = fdtbus_get_reg(phandle, i, &hp->addr, &hp->size);
		if (error != 0) {
			aprint_error(": couldn't get registers: %u\n", i);
			return error;
		}
		error = bus_space_map(bst, hp->addr, hp->size, 0, &hp->bsh);
		if (error != 0) {
			aprint_error(": couldn't map registers: %d: "
			"0x%" PRIxBUSADDR ": 0x%" PRIxBUSSIZE "\n",
			i, hp->addr, hp->size);
			return error;
		}
	}

	mutex_init(&cru->lock, MUTEX_DEFAULT, IPL_VM);

	return 0;
}

uint32_t
st_cru_read(u_int handle, bus_size_t offset)
{

	KASSERT(cru != NULL);
	KASSERT(handle < cru->nhandles);
	return bus_space_read_4(cru->bst, cru->handles[handle].bsh, offset);
}

void
st_cru_write(u_int handle, bus_size_t offset, uint32_t val)
{

	KASSERT(cru != NULL);
	KASSERT(handle < cru->nhandles);
	bus_space_write_4(cru->bst, cru->handles[handle].bsh, offset, val);
}

void
st_cru_lock(void)
{

	KASSERT(cru != NULL);
	mutex_enter(&cru->lock);
}

void
st_cru_unlock(void)
{

	KASSERT(cru != NULL);
	mutex_exit(&cru->lock);
}

bool
st_cru_locked(void)
{

	KASSERT(cru != NULL);
	return mutex_owned(&cru->lock);
}
