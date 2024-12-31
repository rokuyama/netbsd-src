#	$NetBSD$

WARNS=	0
PROG=	diff
NOMAN=	# defined

DIST=	${NETBSDSRCDIR}/external/gpl2/diffutils/dist

CPPFLAGS+=	-DLOCALEDIR=\"/usr/share/locale\" -DHAVE_CONFIG_H \
		-I${DIST}/../include -I${DIST}/lib

# libdiffutils
.PATH:	${DIST}/lib

SRCS+=	basename.c c-stack.c cmpbuf.c error.c exclude.c exitfail.c \
	freesoft.c \
	hard-locale.c imaxtostr.c offtostr.c prepargs.c posixver.c quotesys.c \
	regex.c setmode.c strftime.c umaxtostr.c xmalloc.c xstrtoumax.c

CWARNFLAGS.clang+=	-Wno-unused-value -Wno-tautological-compare

# diff
.PATH:	${DIST}/src

SRCS+=	analyze.c context.c diff.c dir.c ed.c ifdef.c io.c \
	normal.c side.c util.c version.c

COPTS.ifdef.c = -Wno-stack-protector

CWARNFLAGS.clang+=	-Wno-unused-value -Wno-string-plus-int -Wno-error=bool-operation

.include <bsd.prog.mk>
