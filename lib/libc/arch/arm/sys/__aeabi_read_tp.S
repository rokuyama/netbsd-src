/* $NetBSD: __aeabi_read_tp.S,v 1.5 2013/08/19 22:10:51 matt Exp $ */

#undef __thumb__
#include "SYS.h"

/*
 * Implementations of this function should corrupt only the result register
 * (r0) and the non-parameter integer core registers allowed to be corrupted
 * by the [AAPCS] (ip, lr, and CPSR).
 */
ARM_ENTRY(__aeabi_read_tp)
#ifdef _ARM_ARCH_6
	mrc	p15, 0, r0, c13, c0, 3
#else
	SYSTRAP(_lwp_getprivate)
#endif
	RET
END(__aeabi_read_tp)
