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

#ifndef __RISCV_SPACEMIT_RESET_H
#define	__RISCV_SPACEMIT_RESET_H

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>

#include <dev/fdt/fdtvar.h>

#include <riscv/spacemit/st_cru.h>

struct st_reset {
	u_int		handle;
	bus_size_t	offset;
	uint32_t	assert_mask;
	uint32_t	deassert_mask;
};

struct st_reset_softc {
	device_t			sc_dev;
	int				sc_phandle;
	bus_space_tag_t			sc_bst;
	u_int				sc_nhandles;
	u_int				sc_nresets;
	const struct st_reset *		sc_resets;
};

void	st_reset_init(struct st_reset_softc *);

#endif /* !__RISCV_SPACEMIT_RESET_H */
