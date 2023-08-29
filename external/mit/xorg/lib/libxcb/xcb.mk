#	$NetBSD: xcb.mk,v 1.5 2019/05/29 13:15:53 maya Exp $

# define XCBEXT to something before including this

LIB=	xcb-${XCBEXT}

SRCS=	${XCBEXT}.c

CPPFLAGS+=	-I${X11SRCDIR.xcb}/src
CPPFLAGS+=	-I${.CURDIR}/../files

LIBDPLIBS=\
	xcb	${.CURDIR}/../libxcb \
	Xau	${.CURDIR}/../../libXau \
	Xdmcp	${.CURDIR}/../../libXdmcp

SHLIB_MAJOR?=	0
SHLIB_MINOR?=	1

PKGCONFIG=		xcb-${XCBEXT}
PKGDIST.xcb-${XCBEXT}=	${X11SRCDIR.xcb}

.include <bsd.x11.mk>
.include <bsd.lib.mk>

.if ${ACTIVE_CC} == "gcc" && ${HAVE_GCC:U0} >= 12
COPTS.xinput.c+=	-Wno-error=alloc-size-larger-than=
.endif

.PATH: ${.CURDIR}/../files ${X11SRCDIR.xcb}
