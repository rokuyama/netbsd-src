#	$NetBSD: Makefile,v 1.2 2019/01/27 02:08:44 pgoyette Exp $

.include <bsd.init.mk>

.PATH:	${S}/compat/common

KMOD=	compat_13

CPPFLAGS+=	-DCOMPAT_13 -DCOMPAT_14 -DCOMPAT_16 -DCOMPAT_20 -DCOMPAT_30
CPPFLAGS+=	-DCOMPAT_40 -DCOMPAT_50 -DCOMPAT_60 -DCOMPAT_70 -DCOMPAT_80

SRCS+=	compat_13_mod.c
SRCS+=	uvm_13.c kern_sig_13.c

COMPAT_MACHDEP_SRCS=	compat_13_machdep.c
.if ${MACHINE_CPU} == "m68k"
COMPAT_MACHDEP_SRCS+=	compat_13_sigreturn13.s
.endif

.include "../Makefile.compat"

.include <bsd.kmodule.mk>
