/* $NetBSD: sun6i_dma.c,v 1.16 2024/08/13 07:20:23 skrll Exp $ */

/*-
 * Copyright (c) 2014-2017 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sun6i_dma.c,v 1.16 2024/08/13 07:20:23 skrll Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/bitops.h>
#include <sys/kmem.h>

#include <dev/fdt/fdtvar.h>

#define DMA_IRQ_EN_REG0_REG		0x0000
#define DMA_IRQ_EN_REG1_REG		0x0004
#define  DMA_IRQ_EN_REG0_QUEUE_IRQ_EN(n)	__BIT(n * 4 + 2)
#define  DMA_IRQ_EN_REG0_PKG_IRQ_EN(n)		__BIT(n * 4 + 1)
#define  DMA_IRQ_EN_REG0_HLAF_IRQ_EN(n)		__BIT(n * 4 + 0)
#define  DMA_IRQ_EN_REG1_QUEUE_IRQ_EN(n)	__BIT((n - 8) * 4 + 2)
#define  DMA_IRQ_EN_REG1_PKG_IRQ_EN(n)		__BIT((n - 8) * 4 + 1)
#define  DMA_IRQ_EN_REG1_HLAF_IRQ_EN(n)		__BIT((n - 8) * 4 + 0)
#define DMA_IRQ_PEND_REG0_REG		0x0010
#define DMA_IRQ_PEND_REG1_REG		0x0014
#define  DMA_IRQ_QUEUE_MASK			0x4444444444444444ULL
#define  DMA_IRQ_PKG_MASK			0x2222222222222222ULL
#define  DMA_IRQ_HF_MASK			0x1111111111111111ULL
#define DMA_STA_REG			0x0030
#define DMA_EN_REG(n)			(0x0100 + (n) * 0x40 + 0x00)
#define  DMA_EN_EN				__BIT(0)
#define DMA_PAU_REG(n)			(0x0100 + (n) * 0x40 + 0x04)
#define  DMA_PAU_PAUSE				__BIT(0)
#define DMA_START_ADDR_REG(n)		(0x0100 + (n) * 0x40 + 0x08)
#define DMA_CFG_REG(n)			(0x0100 + (n) * 0x40 + 0x0C)
#define  DMA_CFG_DEST_DATA_WIDTH		__BITS(26,25)
#define   DMA_CFG_DATA_WIDTH(n)			((n) >> 4)
#define	  DMA_CFG_BST_LEN(n)			((n) == 1 ? 0 : (((n) >> 3) + 1))
#define  DMA_CFG_DEST_ADDR_MODE			__BITS(22,21)
#define   DMA_CFG_ADDR_MODE_LINEAR		0
#define   DMA_CFG_ADDR_MODE_IO			1
#define  DMA_CFG_DEST_DRQ_TYPE			__BITS(20,16)
#define	  DMA_CFG_DRQ_TYPE_SDRAM		1
#define  DMA_CFG_SRC_DATA_WIDTH			__BITS(10,9)
#define  DMA_CFG_SRC_ADDR_MODE			__BITS(6,5)
#define  DMA_CFG_SRC_DRQ_TYPE			__BITS(4,0)
#define DMA_CUR_SRC_REG(n)		(0x0100 + (n) * 0x40 + 0x10)
#define DMA_CUR_DEST_REG(n)		(0x0100 + (n) * 0x40 + 0x14)
#define DMA_BCNT_LEFT_REG(n)		(0x0100 + (n) * 0x40 + 0x18)
#define DMA_PARA_REG(n)			(0x0100 + (n) * 0x40 + 0x1C)
#define  DMA_PARA_DATA_BLK_SIZE			__BITS(15,8)
#define  DMA_PARA_WAIT_CYC			__BITS(7,0)
#define DMA_MODE_REG(n)			(0x0100 + (n) * 0x40 + 0x28)
#define  MODE_WAIT				0b0
#define  MODE_HANDSHAKE				0b1
#define  DMA_MODE_DST(m)			__SHIFTIN((m), __BIT(3))
#define  DMA_MODE_SRC(m)			__SHIFTIN((m), __BIT(2))
#define DMA_FDESC_ADDR_REG(n)		(0x0100 + (n) * 0x40 + 0x2C)
#define DMA_PKG_NUM_REG(n)		(0x0100 + (n) * 0x40 + 0x30)

struct sun6idma_desc {
	uint32_t	dma_config;
	uint32_t	dma_srcaddr;
	uint32_t	dma_dstaddr;
	uint32_t	dma_bcnt;
	uint32_t	dma_para;
	uint32_t	dma_next;
#define DMA_NULL	0xfffff800
};

struct sun6idma_config {
	u_int		num_channels;
	bool		autogate;
	uint8_t		bursts;
	uint8_t		widths;
	bus_size_t	autogate_reg;
	uint32_t	autogate_mask;
	uint32_t	burst_mask;
};

#define IL2B(x)			__BIT(ilog2(x))
#define IL2B_RANGE(x, y)	__BITS(ilog2(x), ilog2(y))
#define WIDTHS_1_2_4		IL2B_RANGE(4, 1)
#define WIDTHS_1_2_4_8		IL2B_RANGE(8, 1)
#define BURSTS_1_8		(IL2B(8)|IL2B(1))
#define BURSTS_1_4_8_16		(IL2B(16)|IL2B(8)|IL2B(4)|IL2B(1))

static const struct sun6idma_config sun6i_a31_dma_config = {
	.num_channels = 16,
	.burst_mask = __BITS(8,7),
	.bursts = BURSTS_1_8,
	.widths = WIDTHS_1_2_4,
};

static const struct sun6idma_config sun8i_a83t_dma_config = {
	.num_channels = 8,
	.autogate = true,
	.autogate_reg = 0x20,
	.autogate_mask = 0x4,
	.burst_mask = __BITS(8,7),
	.bursts = BURSTS_1_8,
	.widths = WIDTHS_1_2_4,
};

static const struct sun6idma_config sun8i_h3_dma_config = {
	.num_channels = 12,
	.autogate = true,
	.autogate_reg = 0x28,
	.autogate_mask = 0x4,
	.burst_mask = __BITS(7,6),
	.bursts = BURSTS_1_4_8_16,
	.widths = WIDTHS_1_2_4_8,
};

static const struct sun6idma_config sun8i_v3s_dma_config = {
	.num_channels = 8,
	.autogate = true,
	.autogate_reg = 0x20,
	.autogate_mask = 0x4,
	.burst_mask = __BITS(8,7),
	.bursts = BURSTS_1_8,
	.widths = WIDTHS_1_2_4,
};

static const struct sun6idma_config sun20i_d1_dma_config = {
	.num_channels = 16,
	.autogate = true,
	.autogate_reg = 0x28,
	.autogate_mask = 0x4,
	.burst_mask = __BITS(7,6),
	.bursts = BURSTS_1_4_8_16,
	.widths = WIDTHS_1_2_4_8,
};

static const struct sun6idma_config sun50i_a64_dma_config = {
	.num_channels = 8,
	.autogate = true,
	.autogate_reg = 0x28,
	.autogate_mask = 0x4,
	.burst_mask = __BITS(7,6),
	.bursts = BURSTS_1_4_8_16,
	.widths = WIDTHS_1_2_4_8,
};

static const struct device_compatible_entry compat_data[] = {
	{ .compat = "allwinner,sun6i-a31-dma",
	  .data = &sun6i_a31_dma_config },
	{ .compat = "allwinner,sun8i-a83t-dma",
	  .data = &sun8i_a83t_dma_config },
	{ .compat = "allwinner,sun8i-h3-dma",
	  .data = &sun8i_h3_dma_config },
	{ .compat = "allwinner,sun8i-v3s-dma",
	  .data = &sun8i_v3s_dma_config },
	{ .compat = "allwinner,sun20i-d1-dma",
	  .data = &sun20i_d1_dma_config },
	{ .compat = "allwinner,sun50i-a64-dma",
	  .data = &sun50i_a64_dma_config },

	DEVICE_COMPAT_EOL
};

struct sun6idma_channel {
	uint8_t			ch_index;
	void			(*ch_callback)(void *);
	void			*ch_callbackarg;
	u_int			ch_portid;
	void			*ch_dmadesc;
};

struct sun6idma_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	bus_dma_tag_t		sc_dmat;
	int			sc_phandle;
	void			*sc_ih;

	uint32_t		sc_burst_mask;

	kmutex_t		sc_lock;

	struct sun6idma_channel	*sc_chan;
	u_int			sc_nchan;
	u_int			sc_ndesc_ch;
	uint8_t			sc_widths;
	uint8_t			sc_bursts;

	bus_dma_segment_t	sc_dmasegs[1];
	bus_dmamap_t		sc_dmamap;
	void			*sc_dmadescs;
};

#define DMA_READ(sc, reg)		\
    bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define DMA_WRITE(sc, reg, val)		\
    bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

#define DESC_NUM			((MAXPHYS / MIN_PAGE_SIZE + 1) + 1)
#define DESC_LEN(n)			\
    (sizeof(struct sun6idma_desc) * (n))
#define DESC_OFFS(ch, n)		\
    ((ch) * roundup2(DESC_LEN(DESC_NUM), COHERENCY_UNIT) + DESC_LEN(n))
#define DESC_ADDR(sc, chp, n)		\
    ((sc)->sc_dmamap->dm_segs[0].ds_addr + DESC_OFFS((chp)->ch_index, (n)))

static void *
sun6idma_acquire(device_t dev, const void *data, size_t len,
    void (*cb)(void *), void *cbarg)
{
	struct sun6idma_softc *sc = device_private(dev);
	struct sun6idma_channel *ch = NULL;
	uint32_t irqen;
	uint8_t index;

	if (len != 4)
		return NULL;

	const u_int portid = be32dec(data);
	if (portid > __SHIFTOUT_MASK(DMA_CFG_SRC_DRQ_TYPE))
		return NULL;

	mutex_enter(&sc->sc_lock);

	for (index = 0; index < sc->sc_nchan; index++) {
		if (sc->sc_chan[index].ch_callback == NULL) {
			ch = &sc->sc_chan[index];
			ch->ch_callback = cb;
			ch->ch_callbackarg = cbarg;
			ch->ch_portid = portid;

			irqen = DMA_READ(sc, index < 8 ?
			    DMA_IRQ_EN_REG0_REG :
			    DMA_IRQ_EN_REG1_REG);
			irqen |= (index < 8 ?
			    DMA_IRQ_EN_REG0_PKG_IRQ_EN(index) :
			    DMA_IRQ_EN_REG1_PKG_IRQ_EN(index));
			DMA_WRITE(sc, index < 8 ?
			    DMA_IRQ_EN_REG0_REG :
			    DMA_IRQ_EN_REG1_REG, irqen);

			break;
		}
	}

	mutex_exit(&sc->sc_lock);

	return ch;
}

static void
sun6idma_release(device_t dev, void *priv)
{
	struct sun6idma_softc *sc = device_private(dev);
	struct sun6idma_channel *ch = priv;
	uint32_t irqen;
	uint8_t index = ch->ch_index;

	mutex_enter(&sc->sc_lock);

	irqen = DMA_READ(sc, index < 8 ?
	    DMA_IRQ_EN_REG0_REG :
	    DMA_IRQ_EN_REG1_REG);
	irqen &= ~(index < 8 ?
	    DMA_IRQ_EN_REG0_PKG_IRQ_EN(index) :
	    DMA_IRQ_EN_REG1_PKG_IRQ_EN(index));
	DMA_WRITE(sc, index < 8 ?
	    DMA_IRQ_EN_REG0_REG :
	    DMA_IRQ_EN_REG1_REG, irqen);

	ch->ch_callback = NULL;
	ch->ch_callbackarg = NULL;

	mutex_exit(&sc->sc_lock);
}

static int
sun6idma_transfer(device_t dev, void *priv, struct fdtbus_dma_req *req)
{
	struct sun6idma_softc *sc = device_private(dev);
	struct sun6idma_channel *ch = priv;
	struct sun6idma_desc *desc = ch->ch_dmadesc;
	uint32_t src, dst, len, cfg, mem_cfg, dev_cfg;
	uint32_t mem_width, dev_width, mem_burst, dev_burst;

	if (req->dreq_nsegs > sc->sc_ndesc_ch)
		return EINVAL;

	if ((sc->sc_widths &
	    IL2B(req->dreq_mem_opt.opt_bus_width/NBBY)) == 0)
		return EINVAL;
	if ((sc->sc_widths &
	    IL2B(req->dreq_dev_opt.opt_bus_width/NBBY)) == 0)
		return EINVAL;
	if ((sc->sc_bursts &
	    IL2B(req->dreq_mem_opt.opt_burst_len)) == 0)
		return EINVAL;
	if ((sc->sc_bursts &
	    IL2B(req->dreq_dev_opt.opt_burst_len)) == 0)
		return EINVAL;

	mem_width = DMA_CFG_DATA_WIDTH(req->dreq_mem_opt.opt_bus_width);
	dev_width = DMA_CFG_DATA_WIDTH(req->dreq_dev_opt.opt_bus_width);
	mem_burst = DMA_CFG_BST_LEN(req->dreq_mem_opt.opt_burst_len);
	dev_burst = DMA_CFG_BST_LEN(req->dreq_dev_opt.opt_burst_len);

	mem_cfg = __SHIFTIN(mem_width, DMA_CFG_SRC_DATA_WIDTH) |
	    __SHIFTIN(mem_burst, sc->sc_burst_mask) |
	    __SHIFTIN(DMA_CFG_ADDR_MODE_LINEAR, DMA_CFG_SRC_ADDR_MODE) |
	    __SHIFTIN(DMA_CFG_DRQ_TYPE_SDRAM, DMA_CFG_SRC_DRQ_TYPE);
	dev_cfg = __SHIFTIN(dev_width, DMA_CFG_SRC_DATA_WIDTH) |
	    __SHIFTIN(dev_burst, sc->sc_burst_mask) |
	    __SHIFTIN(DMA_CFG_ADDR_MODE_IO, DMA_CFG_SRC_ADDR_MODE) |
	    __SHIFTIN(ch->ch_portid, DMA_CFG_SRC_DRQ_TYPE);

	for (size_t j = 0; j < req->dreq_nsegs; j++) {
		if (req->dreq_dir == FDT_DMA_READ) {
			src = req->dreq_dev_phys;
			dst = req->dreq_segs[j].ds_addr;
			cfg = mem_cfg << 16 | dev_cfg;
		} else {
			src = req->dreq_segs[j].ds_addr;
			dst = req->dreq_dev_phys;
			cfg = dev_cfg << 16 | mem_cfg;
		}
		len = req->dreq_segs[j].ds_len;

		desc[j].dma_config = htole32(cfg);
		desc[j].dma_srcaddr = htole32(src);
		desc[j].dma_dstaddr = htole32(dst);
		desc[j].dma_bcnt = htole32(len);
		desc[j].dma_para = htole32(0);
		if (j < req->dreq_nsegs - 1)
			desc[j].dma_next = htole32(DESC_ADDR(sc, ch, j + 1));
		else
			desc[j].dma_next = htole32(DMA_NULL);
	}

#if notyet && maybenever
	DMA_WRITE(sc, DMA_MODE_REG(ch->ch_index),
	    DMA_MODE_DST(MODE_HANDSHAKE)|DMA_MODE_SRC(MODE_HANDSHAKE));
#endif

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, DESC_OFFS(ch->ch_index, 0),
	    DESC_LEN(req->dreq_nsegs), BUS_DMASYNC_PREWRITE);

	DMA_WRITE(sc, DMA_START_ADDR_REG(ch->ch_index),
	    DESC_ADDR(sc, ch, 0));
	DMA_WRITE(sc, DMA_EN_REG(ch->ch_index), DMA_EN_EN);

	if ((DMA_READ(sc, DMA_EN_REG(ch->ch_index)) & DMA_EN_EN) == 0) {
		aprint_error_dev(sc->sc_dev,
		    "DMA Channel %u failed to start\n", ch->ch_index);
		return EIO;
	}

	return 0;
}

static void
sun6idma_halt(device_t dev, void *priv)
{
	struct sun6idma_softc *sc = device_private(dev);
	struct sun6idma_channel *ch = priv;

	DMA_WRITE(sc, DMA_EN_REG(ch->ch_index), 0);
}

static const struct fdtbus_dma_controller_func sun6idma_funcs = {
	.acquire = sun6idma_acquire,
	.release = sun6idma_release,
	.transfer = sun6idma_transfer,
	.halt = sun6idma_halt
};

static int
sun6idma_intr(void *priv)
{
	struct sun6idma_softc *sc = priv;
	uint32_t pend0, pend1, bit;
	uint64_t pend, mask;
	uint8_t index;

	pend0 = DMA_READ(sc, DMA_IRQ_PEND_REG0_REG);
	pend1 = DMA_READ(sc, DMA_IRQ_PEND_REG1_REG);
	if (!pend0 && !pend1)
		return 0;

	DMA_WRITE(sc, DMA_IRQ_PEND_REG0_REG, pend0);
	DMA_WRITE(sc, DMA_IRQ_PEND_REG1_REG, pend1);

	pend = pend0 | ((uint64_t)pend1 << 32);

	while ((bit = ffs64(pend & DMA_IRQ_PKG_MASK)) != 0) {
		mask = __BIT(bit - 1);
		pend &= ~mask;
		index = (bit - 1) / 4;

		if (sc->sc_chan[index].ch_callback == NULL)
			continue;
		sc->sc_chan[index].ch_callback(
		    sc->sc_chan[index].ch_callbackarg);
	}

	return 1;
}

static int
sun6idma_match(device_t parent, cfdata_t cf, void *aux)
{
	struct fdt_attach_args * const faa = aux;

	return of_compatible_match(faa->faa_phandle, compat_data);
}

static void
sun6idma_attach(device_t parent, device_t self, void *aux)
{
	struct sun6idma_softc * const sc = device_private(self);
	struct fdt_attach_args * const faa = aux;
	const int phandle = faa->faa_phandle;
	size_t desclen;
	const struct sun6idma_config *conf;
	struct fdtbus_reset *rst;
	struct clk *clk;
	char intrstr[128];
	bus_addr_t addr;
	bus_size_t size;
	int error, nsegs;
	u_int index;

	if (fdtbus_get_reg(phandle, 0, &addr, &size) != 0) {
		aprint_error(": couldn't get registers\n");
		return;
	}

	if ((clk = fdtbus_clock_get_index(phandle, 0)) == NULL ||
	    clk_enable(clk) != 0) {
		aprint_error(": couldn't enable clock\n");
		return;
	}
	if ((rst = fdtbus_reset_get_index(phandle, 0)) == NULL ||
	    fdtbus_reset_deassert(rst) != 0) {
		aprint_error(": couldn't de-assert reset\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_phandle = phandle;
	sc->sc_dmat = faa->faa_dmat;
	sc->sc_bst = faa->faa_bst;
	if (bus_space_map(sc->sc_bst, addr, size, 0, &sc->sc_bsh) != 0) {
		aprint_error(": couldn't map registers\n");
		return;
	}
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SCHED);

	if (!fdtbus_intr_str(phandle, 0, intrstr, sizeof(intrstr))) {
		aprint_error(": failed to decode interrupt\n");
		return;
	}

	conf = of_compatible_lookup(phandle, compat_data)->data;

	sc->sc_burst_mask = conf->burst_mask;
	sc->sc_nchan = conf->num_channels;
	sc->sc_widths = conf->widths;
	sc->sc_bursts = conf->bursts;
	sc->sc_chan = kmem_alloc(sizeof(*sc->sc_chan) * sc->sc_nchan, KM_SLEEP);
	desclen = DESC_OFFS(sc->sc_nchan, 0);
	sc->sc_ndesc_ch = DESC_OFFS(1, 0) / sizeof(struct sun6idma_desc);

	aprint_naive("\n");
	aprint_normal(": DMA controller (%u channels)\n", sc->sc_nchan);

	DMA_WRITE(sc, DMA_IRQ_EN_REG0_REG, 0);
	DMA_WRITE(sc, DMA_IRQ_EN_REG1_REG, 0);
	DMA_WRITE(sc, DMA_IRQ_PEND_REG0_REG, ~0);
	DMA_WRITE(sc, DMA_IRQ_PEND_REG1_REG, ~0);

	error = bus_dmamem_alloc(sc->sc_dmat, desclen, 0, 0,
	    sc->sc_dmasegs, 1, &nsegs, BUS_DMA_WAITOK);
	if (error)
		panic("bus_dmamem_alloc failed: %d", error);
	error = bus_dmamem_map(sc->sc_dmat, sc->sc_dmasegs, nsegs,
	    desclen, (void **)&sc->sc_dmadescs, BUS_DMA_WAITOK);
	if (error)
		panic("bus_dmamem_map failed: %d", error);
	error = bus_dmamap_create(sc->sc_dmat, desclen, 1, desclen, 0,
	    BUS_DMA_WAITOK, &sc->sc_dmamap);
	if (error)
		panic("bus_dmamap_create failed: %d", error);
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap,
	    sc->sc_dmadescs, desclen, NULL, BUS_DMA_WAITOK);
	if (error)
		panic("bus_dmamap_load failed: %d", error);

	for (index = 0; index < sc->sc_nchan; index++) {
		struct sun6idma_channel *ch = &sc->sc_chan[index];
		ch->ch_index = index;
		ch->ch_dmadesc = (void *)((uintptr_t)sc->sc_dmadescs + DESC_OFFS(index, 0));
		ch->ch_callback = NULL;
		ch->ch_callbackarg = NULL;

		DMA_WRITE(sc, DMA_EN_REG(index), 0);
	}

	if (conf->autogate)
		DMA_WRITE(sc, conf->autogate_reg, conf->autogate_mask);

	sc->sc_ih = fdtbus_intr_establish_xname(phandle, 0, IPL_SCHED,
	    FDT_INTR_MPSAFE, sun6idma_intr, sc, device_xname(sc->sc_dev));
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish interrupt on %s\n", intrstr);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting on %s\n", intrstr);

	fdtbus_register_dma_controller(self, phandle, &sun6idma_funcs);
}

CFATTACH_DECL_NEW(sun6i_dma, sizeof(struct sun6idma_softc),
        sun6idma_match, sun6idma_attach, NULL, NULL);

#ifdef DDB
void sun6idma_dump(void);

void
sun6idma_dump(void)
{
	struct sun6idma_softc *sc;
	device_t dev;
	u_int index;

	dev = device_find_by_driver_unit("sun6idma", 0);
	if (dev == NULL)
		return;
	sc = device_private(dev);

	device_printf(dev, "DMA_IRQ_EN_REG0_REG:   %08x\n", DMA_READ(sc, DMA_IRQ_EN_REG0_REG));
	device_printf(dev, "DMA_IRQ_EN_REG1_REG:   %08x\n", DMA_READ(sc, DMA_IRQ_EN_REG1_REG));
	device_printf(dev, "DMA_IRQ_PEND_REG0_REG: %08x\n", DMA_READ(sc, DMA_IRQ_PEND_REG0_REG));
	device_printf(dev, "DMA_IRQ_PEND_REG1_REG: %08x\n", DMA_READ(sc, DMA_IRQ_PEND_REG1_REG));
	device_printf(dev, "DMA_STA_REG:           %08x\n", DMA_READ(sc, DMA_STA_REG));

	for (index = 0; index < sc->sc_nchan; index++) {
		struct sun6idma_channel *ch = &sc->sc_chan[index];
		if (ch->ch_callback == NULL)
			continue;
		device_printf(dev, " %2d: DMA_EN_REG:         %08x\n", index, DMA_READ(sc, DMA_EN_REG(index)));
		device_printf(dev, " %2d: DMA_PAU_REG:        %08x\n", index, DMA_READ(sc, DMA_PAU_REG(index)));
		device_printf(dev, " %2d: DMA_START_ADDR_REG: %08x\n", index, DMA_READ(sc, DMA_START_ADDR_REG(index)));
		device_printf(dev, " %2d: DMA_CFG_REG:        %08x\n", index, DMA_READ(sc, DMA_CFG_REG(index)));
		device_printf(dev, " %2d: DMA_CUR_SRC_REG:    %08x\n", index, DMA_READ(sc, DMA_CUR_SRC_REG(index)));
		device_printf(dev, " %2d: DMA_CUR_DEST_REG:   %08x\n", index, DMA_READ(sc, DMA_CUR_DEST_REG(index)));
		device_printf(dev, " %2d: DMA_BCNT_LEFT_REG:  %08x\n", index, DMA_READ(sc, DMA_BCNT_LEFT_REG(index)));
		device_printf(dev, " %2d: DMA_PARA_REG:       %08x\n", index, DMA_READ(sc, DMA_PARA_REG(index)));
		device_printf(dev, " %2d: DMA_MODE_REG:       %08x\n", index, DMA_READ(sc, DMA_MODE_REG(index)));
		device_printf(dev, " %2d: DMA_FDESC_ADDR_REG: %08x\n", index, DMA_READ(sc, DMA_FDESC_ADDR_REG(index)));
		device_printf(dev, " %2d: DMA_PKG_NUM_REG:    %08x\n", index, DMA_READ(sc, DMA_PKG_NUM_REG(index)));
	}
}
#endif
