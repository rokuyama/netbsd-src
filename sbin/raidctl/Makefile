#	$NetBSD: Makefile,v 1.18 2023/06/03 09:09:13 lukem Exp $

RUMPPRG=raidctl 
SRCS=	rf_configure.c raidctl.c
MAN=	raidctl.8

DPADD=  ${LIBUTIL}
LDADD=  -lutil

COPTS.raidctl.c+=	${CC_WNO_STRINGOP_TRUNCATION}

.include <bsd.own.mk>
# XXXRO sh3 -O1 gcc 10.5.0
.if ${ACTIVE_CC} == "gcc" && ${HAVE_GCC:U0} >= 10
COPTS.raidctl.c+=	-Wno-error=format-truncation
.endif

.include <bsd.prog.mk>
