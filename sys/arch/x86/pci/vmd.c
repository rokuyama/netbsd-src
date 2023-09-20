/*	$NetBSD$	*/

/*-
 * Copyright (c) 2023 Rin Okuyama <rin@NetBSD.org>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#ifdef _KERNEL_OPT
#endif

#if defined(DEBUG) && !defined(VMD_DEBUG)
#define	VMD_DEBUG
#endif

#ifdef VMD_DEBUG
#define	DPRINTF(fmt, args...)	printf(fmt, ##args)
#else
#define	DPRINTF(fmt, args...)	/* nothing */
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

enum vmd_feat {
	VMD_FEAT_NONE		= 0,
	VMD_FEAT_BUS_RESTRICT	= __BIT(1),
	VMD_FEAT_VECTOR_OFFSET	= __BIT(2),
	VMD_FEAT_BYPASS_MSI	= __BIT(3),
};

struct vmd_intr {
	pci_intr_handle_t	ih;
	const struct pci_attach_args *pa;
	void *			cookie;
	pci_intr_type_t		type;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	bus_size_t		bss;
	int			vec;
};

struct vmd_softc {
	device_t		sc_dev;
	struct pci_attach_args *sc_pa;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	enum vmd_feat		sc_feats;
	int			sc_bus;
	int			sc_sub;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_addr_t		sc_ioaddr;
	bus_size_t		sc_iosize;
	int			sc_vector_offset;
	int			sc_msix_count;
	struct vmd_intr *	sc_intr;
};

static const struct vmd_product {
	pci_vendor_id_t		vendor;
	pci_product_id_t	product;
	enum vmd_feat		feats;
} vmd_products[] = {
	{PCI_VENDOR_INTEL, 0x201d,
	    VMD_FEAT_NONE},
	{PCI_VENDOR_INTEL, 0x28c0,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_BYPASS_MSI},
	{PCI_VENDOR_INTEL, 0x467f,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INTEL, 0x4c3d,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INTEL, 0x7d0b,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INTEL, 0x9a0b,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INTEL, 0xa77f,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INTEL, 0xad0b,
	    VMD_FEAT_BUS_RESTRICT | VMD_FEAT_VECTOR_OFFSET},
	{PCI_VENDOR_INVALID, 0, 0}
};

static int	vmd_match(device_t, cfdata_t, void *);
static void	vmd_attach(device_t, device_t, void *);
static int	vmd_detach(device_t, int);
static void	vmd_childdet(device_t, device_t);

static bool	vmd_suspend(device_t, const pmf_qual_t *);
static bool	vmd_resume(device_t, const pmf_qual_t *);

static pcireg_t	vmd_conf_read(void *, pci_chipset_tag_t, pcitag_t, int);
static void	vmd_conf_write(void *, pci_chipset_tag_t, pcitag_t, int,
		    pcireg_t);

static int	vmd_intr_alloc(void *, const struct pci_attach_args *,
		    pci_intr_handle_t **, int *, pci_intr_type_t);
static void *	vmd_intr_establish_xname(void *, pci_chipset_tag_t,
		    pci_intr_handle_t, int, int (*)(void *), void *,
		    const char *);
static void	vmd_intr_disestablish(void *, pci_chipset_tag_t, void *);
static pci_intr_type_t
		vmd_intr_type(void *, pci_chipset_tag_t, pci_intr_handle_t);
static const char *
		vmd_intr_string(void *, pci_chipset_tag_t, pci_intr_handle_t,
		    char *, size_t);

CFATTACH_DECL3_NEW(vmd, sizeof(struct vmd_softc),
    vmd_match, vmd_attach, vmd_detach, NULL, NULL, vmd_childdet,
    DVF_DETACH_SHUTDOWN);

static const struct vmd_product *
vmd_lookup(const struct pci_attach_args *pa)
{
	const struct vmd_product *vpp;

	for (vpp = vmd_products; vpp->vendor != PCI_VENDOR_INVALID; vpp++) {
		if (PCI_VENDOR(pa->pa_id)  == vpp->vendor &&
		    PCI_PRODUCT(pa->pa_id) == vpp->product)
			return vpp;
	}
	return NULL;
}

static int
vmd_set_bus_range(struct vmd_softc *sc)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag = sc->sc_tag;
	pcireg_t cap, cfg;

#define VMD_CAP_REG			0x0040
#define  VMD_CAP_BUS_RESTRICT		__BIT(0)

#define VMD_CONFIG_REG			0x0044
#define  VMD_CONFIG_BYPASS_MSI		__BIT(1)
#define  VMD_CONFIG_BUS_START_MASK	__BITS(8, 9)

	sc->sc_bus = 0;

	if ((sc->sc_feats & VMD_FEAT_BUS_RESTRICT) == 0)
		goto out;

	cap = pci_conf_read(pc, tag, VMD_CAP_REG);
	if ((cap & VMD_CAP_BUS_RESTRICT) == 0)
		goto out;

	cfg = pci_conf_read(pc, tag, VMD_CONFIG_REG);
	switch (__SHIFTOUT(cfg, VMD_CONFIG_BUS_START_MASK)) {
	case 0x0:
		sc->sc_bus = 0;
		break;
	case 0x1:
		sc->sc_bus = 128;
		break;
	case 0x2:
		sc->sc_bus = 224;
		break;
	default:
		return ENODEV;
	}

 out:
#define	VMD_IOSIZE_PER_BUS	((bus_size_t)1 << 20)
	sc->sc_sub =
	    uimin(sc->sc_bus + sc->sc_iosize / VMD_IOSIZE_PER_BUS, 256);

	aprint_normal_dev(sc->sc_dev, "descendant bus range %d-%d\n",
	    sc->sc_bus, sc->sc_sub - 1);

	return 0;
}

static int
vmd_pci_chipset_tag_create(struct vmd_softc *sc, pci_chipset_tag_t *pcp)
{
	struct pci_overrides *ov;
	uint64_t present = 0;
	int error;

	ov = kmem_zalloc(sizeof(*ov), KM_SLEEP);

	present |= PCI_OVERRIDE_CONF_READ | PCI_OVERRIDE_CONF_WRITE;
	ov->ov_conf_read = vmd_conf_read;
	ov->ov_conf_write = vmd_conf_write;

	present |= PCI_OVERRIDE_INTR_ALLOC |
	    PCI_OVERRIDE_INTR_ESTABLISH_XNAME |
	    PCI_OVERRIDE_INTR_DISESTABLISH |
	    PCI_OVERRIDE_INTR_TYPE |
	    PCI_OVERRIDE_INTR_STRING;
	ov->ov_intr_alloc = vmd_intr_alloc;
	ov->ov_intr_establish_xname = vmd_intr_establish_xname;
	ov->ov_intr_disestablish = vmd_intr_disestablish;
	ov->ov_intr_type = vmd_intr_type;
	ov->ov_intr_string = vmd_intr_string;

	error = pci_chipset_tag_create(sc->sc_pc, present, ov, sc, pcp);
	if (error)
		kmem_free(ov, sizeof(*ov));
	return error;
}

static int
vmd_attach_child(struct vmd_softc *sc)
{
	struct pci_attach_args *pa = sc->sc_pa;
	struct pcibus_attach_args pba;
	int error;

	memset(&pba, 0, sizeof(pba));

	pba.pba_iot = pa->pa_iot;
	pba.pba_memt = pa->pa_memt;
	pba.pba_dmat = pa->pa_dmat;
	pba.pba_dmat64 = pa->pa_dmat64;
	error = vmd_pci_chipset_tag_create(sc, &pba.pba_pc);
	if (error != 0)
		return error;
#if 0 /* XXXRO */
	pba.pba_flags = pa->pa_flags & ~PCI_FLAGS_MRM_OKAY;
#else
	pba.pba_flags = pa->pa_flags;
#endif
	pba.pba_bus = sc->sc_bus;
	pba.pba_sub = sc->sc_sub;
	pba.pba_bridgetag = &sc->sc_tag;
	pba.pba_intrswiz = pa->pa_intrswiz;
	pba.pba_intrtag = pa->pa_intrtag;

	(void)config_found(sc->sc_dev, &pba, pcibusprint,
	    CFARGS(.devhandle = device_handle(sc->sc_dev)));

	return 0;
}

static void
vmd_set_msi_bypass(struct vmd_softc *sc, bool enable)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag = sc->sc_tag;
	pcireg_t old, new;

	old = pci_conf_read(pc, tag, VMD_CONFIG_REG);
	if (enable)
		new = old | VMD_CONFIG_BYPASS_MSI;
	else
		new = old & ~VMD_CONFIG_BYPASS_MSI;
	if (new != old)
		pci_conf_write(pc, tag, VMD_CONFIG_REG, new);
}

static int
vmd_null_intrhand(void *arg)
{
#ifdef VMD_DEBUG
	static uint64_t count;

	DPRINTF("%s: count %zu\n", __func__, count++);
#endif
	return 1;
}

static int
vmd_intr_init(struct vmd_softc *sc)
{
	struct pci_attach_args *pa = sc->sc_pa;
	enum vmd_feat feats = sc->sc_feats;
	struct vmd_intr *vip;
	pci_intr_handle_t *ihp;
	int count, i, error;

	if ((feats & VMD_FEAT_BYPASS_MSI) != 0) {
		sc->sc_msix_count = 0;
		vmd_set_msi_bypass(sc, true);
		aprint_normal_dev(sc->sc_dev,
		    "interrupts bypassed to parent\n");
		return 0;
	}

	if ((feats & VMD_FEAT_VECTOR_OFFSET) != 0)
		sc->sc_vector_offset = 1;

	count = pci_msix_count(pa->pa_pc, pa->pa_tag);
	KASSERT(count > sc->sc_vector_offset);

	error = pci_msix_alloc(pa, &ihp, &count);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to alloc msix vectors\n");
		return error;
	}
	KASSERT(count > sc->sc_vector_offset);

#define	VMD_INTR_TYPE_NONE	PCI_INTR_TYPE_SIZE
#define	VMD_INTR_AVAIL(sc, i)	((sc)->sc_intr[i].type == VMD_INTR_TYPE_NONE)

	sc->sc_msix_count = count;
	sc->sc_intr = kmem_zalloc(sizeof(sc->sc_intr[0]) * count, KM_SLEEP);
	for (i = 0; i < count; i++) {
		vip = &sc->sc_intr[i];
		vip->ih = ihp[i];
		vip->type = VMD_INTR_TYPE_NONE;
		if (i < sc->sc_vector_offset) {
			vip->cookie = pci_intr_establish_xname(sc->sc_pc,
			    vip->ih, IPL_BIO /* XXXRO */,
			    vmd_null_intrhand, NULL,
			    device_xname(sc->sc_dev));
			KASSERT(vip->cookie != NULL);
		}
	}

	vmd_set_msi_bypass(sc, false);

	aprint_normal_dev(sc->sc_dev, "%d msix vectors allocated",
	    sc->sc_msix_count);
	if (sc->sc_vector_offset != 0)
		aprint_normal(", %d reserved for internal use\n",
		    sc->sc_vector_offset);
	else
		aprint_normal("\n");

	return 0;
}

static int
vmd_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args * const pa = aux;

	if (vmd_lookup(pa) != NULL)
		return 1;

	return 0;
}

static void
vmd_attach(device_t parent, device_t self, void *aux)
{
	struct vmd_softc * const sc = device_private(self);
	struct pci_attach_args * const pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	const struct vmd_product *vpp;
	pcireg_t csr, memtype;

	sc->sc_dev = self;
	sc->sc_pa = pa;
	sc->sc_pc = pc;
	sc->sc_tag = tag;

	vpp = vmd_lookup(pa);
	KASSERT(vpp != NULL);
	sc->sc_feats = vpp->feats;

	pci_aprint_devinfo_fancy(pa, "Intel Volume Management Device (VMD)",
	    NULL, 1);

	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if ((csr & PCI_COMMAND_MASTER_ENABLE) == 0) {
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
		    csr | PCI_COMMAND_MASTER_ENABLE);
	}

	memtype = pci_mapreg_type(pc, tag, PCI_BAR0);
	if (pci_mapreg_map(pa, PCI_BAR0, memtype, 0,
	    &sc->sc_iot, &sc->sc_ioh, &sc->sc_ioaddr, &sc->sc_iosize)) {
		aprint_error_dev(self, "unable to map register\n");
		goto reset_csr;
	}

	if (vmd_set_bus_range(sc) != 0) {
		aprint_error_dev(self, "unknown bus offset\n");
		goto unmap_reg;
	}

	if (vmd_intr_init(sc) != 0) {
		aprint_error_dev(self, "unable to initialize intrs\n");
		goto unmap_reg;
	}

	if (vmd_attach_child(sc) != 0) {
		aprint_error_dev(self, "unable to attach child\n");
		goto destroy_intr;
	}

	if (!pmf_device_register(self, vmd_suspend, vmd_resume)) {
		aprint_verbose_dev(self,
		    "unable to establish power handler\n");
	}

	return;

 destroy_intr:
	/* XXXRO not yet */

 unmap_reg:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);

 reset_csr:
	if ((csr & PCI_COMMAND_MASTER_ENABLE) == 0)
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

	return;
}

static int
vmd_detach(device_t self, int flags)
{
//	struct vmd_softc * const sc = device_private(self);
	int error;

	/* XXXRO not yet */

	error = config_detach_children(self, flags);
	if (error != 0)
		return error;

	pmf_device_deregister(self);
	return 0;
}

static void
vmd_childdet(device_t self, device_t child)
{
//	struct vmd_softc * const sc = device_private(self);

	/* XXXRO not yet */

	return;
}

static bool
vmd_suspend(device_t self, const pmf_qual_t *qual)
{
//	struct vmd_softc * const sc = device_private(self);

	/* XXXRO not yet */

	return true;
}

static bool
vmd_resume(device_t self, const pmf_qual_t *qual)
{
//	struct vmd_softc * const sc = device_private(self);

	/* XXXRO not yet */

	return true;
}

static bus_size_t
vmd_conf_offset(struct vmd_softc * const sc,
    pci_chipset_tag_t pc, pcitag_t tag)
{
	int b, d, f;

	pci_decompose_tag(pc, tag, &b, &d, &f);

	if (b < sc->sc_bus || b >= sc->sc_sub)
		return (bus_size_t)-1;

	return ((b - sc->sc_bus) << 20) | (d << 15) | (f << 12);
}

static uint32_t
vmd_read(struct vmd_softc *sc, bus_size_t offset)
{

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, offset);
}

static void
vmd_write(struct vmd_softc *sc, bus_size_t offset, uint32_t val)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, offset, val);
}

static pcireg_t
vmd_conf_read(void *ctx, pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	struct vmd_softc * const sc = ctx;
	bus_size_t offset;

	offset = vmd_conf_offset(sc, pc, tag);
	if (offset == (bus_size_t)-1)
		return (pcireg_t)-1;

	return vmd_read(sc, offset + reg);
}

static void
vmd_conf_write(void *ctx, pci_chipset_tag_t pc, pcitag_t tag, int reg,
    pcireg_t val)
{
	struct vmd_softc * const sc = ctx;
	bus_size_t offset;

	offset = vmd_conf_offset(sc, pc, tag);
	if (offset == (bus_size_t)-1)
		return;

	vmd_write(sc, offset + reg, val);
}

static pci_intr_handle_t *
vmd_alloc_vectors(struct vmd_softc *sc, int *count)
{
	pci_intr_handle_t *ihp;
	int i, resid;

	for (i = sc->sc_vector_offset, resid = *count;
	     i < sc->sc_msix_count && resid > 0; i++) {
		if (VMD_INTR_AVAIL(sc, i))
			resid--;
	}
	if (resid == *count)
		return NULL;

	*count -= resid;

	ihp = kmem_zalloc(sizeof(*ihp) * (*count), KM_SLEEP);
	return ihp;
}

static int
vmd_map_msix_table(const struct pci_attach_args *pa,
    bus_space_tag_t *bstp, bus_space_handle_t *bshp, bus_size_t *bssp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t tbl, memtype;
	bus_space_tag_t bst = pa->pa_memt;
	bus_space_handle_t bsh;
	bus_addr_t memaddr;
	bus_size_t bss;
	int table_nentry, off, table_offset, bar, flags, error;

	table_nentry = pci_msix_count(pc, tag);
	if (table_nentry == 0) {
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return ENODEV;
	}
	if (pci_get_capability(pc, tag, PCI_CAP_MSIX, &off, NULL) == 0) {
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return ENODEV;
	}

	tbl = pci_conf_read(pc, tag, off + PCI_MSIX_TBLOFFSET);
	table_offset = tbl & PCI_MSIX_TBLOFFSET_MASK;
	bar = PCI_BAR(tbl & PCI_MSIX_TBLBIR_MASK);
	if (bar > PCI_BAR5) {
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return ENODEV;
	}

	memtype = pci_mapreg_type(pc, tag, bar);
	bss = roundup(table_nentry * PCI_MSIX_TABLE_ENTRY_SIZE, PAGE_SIZE);
	if (pci_mapreg_info(pc, tag, bar, memtype, &memaddr, NULL, &flags)
	    != 0)
	{
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return ENODEV;
	}
	if ((flags & BUS_SPACE_MAP_PREFETCHABLE) != 0) {
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		flags &= ~BUS_SPACE_MAP_PREFETCHABLE;
	}

	error = _x86_memio_map(bst, memaddr + table_offset, bss, flags, &bsh);
	if (error != 0) {
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return ENODEV;
	}

	*bstp = bst;
	*bshp = bsh;
	*bssp = bss;
	return 0;
}

/*
 * XXXRO mess!!
 */
#define	MPSAFE_MASK		0x80000000ULL
#define	VMD_INDEX_TO_IH(i)	((pci_intr_handle_t)(i) | __BIT(32))
#define	VMD_IH_TO_INDEX(ih)	((int)((ih) & ~MPSAFE_MASK))
#define	VMD_IH_IS_MPSAFE(ih)	(((ih) & MPSAFE_MASK) != 0)

static int
vmd_reserve_intr(struct vmd_softc *sc, const struct pci_attach_args *pa,
    pci_intr_handle_t **ihps, int *count, pci_intr_type_t type)
{
	bus_space_tag_t bst = (uintptr_t)0;
	bus_space_handle_t bsh = (uintptr_t)0;
	bus_size_t bss = (uintptr_t)0;
	struct vmd_intr *vip;
	pci_intr_handle_t *ihp;
	int i, j;

	ihp = vmd_alloc_vectors(sc, count);
	if (ihp == NULL)
		return ENODEV;

	if (type == PCI_INTR_TYPE_MSIX &&
	    vmd_map_msix_table(pa, &bst, &bsh, &bss) != 0) {
		kmem_free(ihp, sizeof(*ihp) * (*count));
		return ENODEV;
	}

	for (i = sc->sc_vector_offset, j = 0;
	     i < sc->sc_msix_count && j < *count; i++) {
		if (VMD_INTR_AVAIL(sc, i)) {
			vip = &sc->sc_intr[i];
			vip->type = type;
			vip->pa = pa;
			if (type == PCI_INTR_TYPE_MSIX) {
				vip->bst = bst;
				vip->bsh = bsh;
				vip->bss = bss;
				vip->vec = j;
			}
			ihp[j] = VMD_INDEX_TO_IH(i);
			DPRINTF("%s: i %d j %d type %d pa %p ihp %d\n",
			    __func__, i, j, (int)type, pa, (int)ihp[j]);
			j++;
		}
	}
	*ihps = ihp;
	return 0;
}

static int
vmd_intr_alloc(void *ctx, const struct pci_attach_args *pa,
    pci_intr_handle_t **ihps, int *counts, pci_intr_type_t max_type)
{
	struct vmd_softc * const sc = ctx;
	int error, intx_count, msi_count, msix_count;

	if (sc->sc_msix_count == 0)
		return pci_intr_alloc(sc->sc_pa, ihps, counts, max_type);

	intx_count = msi_count = msix_count = 0;
	if (counts == NULL) {
		msix_count = msi_count = intx_count = 1;
	} else {
		switch (max_type) {
		case PCI_INTR_TYPE_MSIX:
			msix_count = counts[PCI_INTR_TYPE_MSIX];
			/* FALLTHROUGH */
		case PCI_INTR_TYPE_MSI:
			msi_count = counts[PCI_INTR_TYPE_MSI];
			/* FALLTHROUGH */
		case PCI_INTR_TYPE_INTX:
			intx_count = counts[PCI_INTR_TYPE_INTX];
			break;
		default:
			return EINVAL;
		}
	}

	if (counts != NULL)
		memset(counts, 0, sizeof(counts[0]) * PCI_INTR_TYPE_SIZE);

	if (msix_count == -1)
		msix_count = pci_msix_count(pa->pa_pc, pa->pa_tag);
	if (msix_count > 0 && vmd_reserve_intr(sc, pa, ihps, &msix_count,
	    PCI_INTR_TYPE_MSIX) == 0) {
		if (counts != NULL)
			counts[PCI_INTR_TYPE_MSIX] = msix_count;
		return 0;
	}

	if (msi_count == -1)
		msi_count = pci_msi_count(pa->pa_pc, pa->pa_tag);
	if (msi_count > 0 && vmd_reserve_intr(sc, pa, ihps, &msi_count,
	    PCI_INTR_TYPE_MSI) == 0) {
		if (counts != NULL)
			counts[PCI_INTR_TYPE_MSI] = msi_count;
		return 0;
	}

	error = EINVAL;
	if (intx_count != 0) {
		KASSERT(intx_count == 1);
		error = pci_intx_alloc(sc->sc_pa, ihps);
		if (error == 0 && counts != NULL)
			counts[PCI_INTR_TYPE_INTX] = 1;
	}
	return error;
}

#define	VMD_INDEX_TO_VEC(sc, i)	(i)

static void
vmd_msix_set_vecctl_mask(struct vmd_softc *sc, int i, bool enable)
{
	struct vmd_intr *vip = &sc->sc_intr[i];
	bus_space_tag_t bst = vip->bst;
	bus_space_handle_t bsh = vip->bsh;
	bus_size_t offset;
	uint32_t vecctl;

	offset = PCI_MSIX_TABLE_ENTRY_SIZE * vip->vec +
	    PCI_MSIX_TABLE_ENTRY_VECTCTL;
	vecctl = bus_space_read_4(bst, bsh, offset);
	if (enable)
		vecctl |= PCI_MSIX_VECTCTL_MASK;
	else
		vecctl &= ~PCI_MSIX_VECTCTL_MASK;
	bus_space_write_4(bst, bsh, offset, vecctl);
	(void)bus_space_read_4(bst, bsh, 0);
}

static int
vmd_msix_addroute(struct vmd_softc *sc, int i)
{
	struct vmd_intr *vip = &sc->sc_intr[i];
	const struct pci_attach_args *pa = vip->pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	bus_space_tag_t bst = vip->bst;
	bus_space_handle_t bsh = vip->bsh;
	int vec = VMD_INDEX_TO_VEC(sc, i);
	pcireg_t ctl;
	bus_size_t entry_base;
	uint32_t addr, data;
	int off, error;

	error = pci_get_capability(pc, tag, PCI_CAP_MSIX, &off, NULL);
	if (error == 0)
		return ENODEV;
	off += PCI_MSIX_CTL;

	ctl = vmd_conf_read(sc, pc, tag, off);
	ctl &= ~PCI_MSIX_CTL_ENABLE;
	vmd_conf_write(sc, pc, tag, off, ctl);

	entry_base = PCI_MSIX_TABLE_ENTRY_SIZE * vip->vec;
#define	LAPIC_MSIADDR_BASE	0xfee00000
#define	VMD_VEC_TO_ADDR(vec)	(LAPIC_MSIADDR_BASE | (vec) << 12);
	addr = VMD_VEC_TO_ADDR(vec);
	data = 0;

	bus_space_write_4(bst, bsh,
	    entry_base + PCI_MSIX_TABLE_ENTRY_ADDR_LO, addr);
	bus_space_write_4(bst, bsh,
	    entry_base + PCI_MSIX_TABLE_ENTRY_ADDR_HI, 0);
	bus_space_write_4(bst, bsh,
	    entry_base + PCI_MSIX_TABLE_ENTRY_DATA, data);
	(void)bus_space_read_4(bst, bsh, 0);

	ctl = vmd_conf_read(sc, pc, tag, off);
	ctl &= ~PCI_MSIX_CTL_FUNCMASK;
	ctl |= PCI_MSIX_CTL_ENABLE;
	vmd_conf_write(sc, pc, tag, off, ctl);

	return 0;
}

static int
vmd_msi_addroute(struct vmd_softc *sc, int i)
{
	struct vmd_intr *vip = &sc->sc_intr[i];
	const struct pci_attach_args *pa = vip->pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	int vec = VMD_INDEX_TO_VEC(sc, i);
	pcireg_t ctl, addr, data;
	int off, error;

	error = pci_get_capability(pc, tag, PCI_CAP_MSI, &off, NULL);
	if (error == 0)
		return ENODEV;

	addr = VMD_VEC_TO_ADDR(vec);
	data = 0;

	ctl = vmd_conf_read(sc, pc, tag, off + PCI_MSI_CTL);
	if ((ctl & PCI_MSI_CTL_64BIT_ADDR) != 0) {
		vmd_conf_write(sc, pc, tag, off + PCI_MSI_MADDR64_LO, addr);
		vmd_conf_write(sc, pc, tag, off + PCI_MSI_MADDR64_HI, 0);
		vmd_conf_write(sc, pc, tag, off + PCI_MSI_MDATA64, data);
	} else {
		vmd_conf_write(sc, pc, tag, off + PCI_MSI_MADDR, addr);
		vmd_conf_write(sc, pc, tag, off + PCI_MSI_MDATA, data);
	}

	ctl |= PCI_MSI_CTL_MSI_ENABLE;
	vmd_conf_write(sc, pc, tag, off + PCI_MSI_CTL, ctl);

	return 0;
}

static void *
vmd_intr_establish_xname(void *ctx, pci_chipset_tag_t pc,
    pci_intr_handle_t ih, int ipl, int (*intrhand)(void *), void *intrarg,
    const char *xname)
{
	struct vmd_softc * const sc = ctx;
	struct vmd_intr *vip;
	char intr_xname[64];
	int i, error;

	DPRINTF("%s: line %d: msix_count: %d\n", __func__, __LINE__,
	    sc->sc_msix_count);

	if (sc->sc_msix_count == 0) {
 bypass:
		return pci_intr_establish(sc->sc_pc, ih, ipl, intrhand,
		    intrarg);
	}

	i = VMD_IH_TO_INDEX(ih);
	vip = &sc->sc_intr[i];
	DPRINTF("%s: i %d ih 0x%016lx\n", __func__, i, ih);
	switch (vip->type) {
	case PCI_INTR_TYPE_MSIX:
		error = vmd_msix_addroute(sc, i);
		if (error != 0) {
			DPRINTF("%s: line %d: error %d\n", __func__,
			    __LINE__, error);
			return NULL;
		}
		vmd_msix_set_vecctl_mask(sc, i, false);
		break;
	case PCI_INTR_TYPE_MSI:
		error = vmd_msi_addroute(sc, i);
		if (error != 0) {
			DPRINTF("%s: line %d: error %d\n", __func__,
			    __LINE__, error);
			return NULL;
		}
		break;
	case PCI_INTR_TYPE_INTX:
		goto bypass;
	default:
		DPRINTF("%s: line %d\n", __func__, __LINE__);
		return NULL;
	}

	if (VMD_IH_IS_MPSAFE(ih) && pci_intr_setattr(sc->sc_pc, &vip->ih,
	    PCI_INTR_MPSAFE, true) != 0) {
#ifdef DIAGNOSTIC
		panic("Unable to set PCI_INTR_MPSAFE\n");
#endif
		return NULL;
	}

	snprintf(intr_xname, sizeof(intr_xname), "%s (%s vec %d)",
	    xname, device_xname(sc->sc_dev), VMD_INDEX_TO_VEC(sc, i));
	vip->cookie = pci_intr_establish_xname(sc->sc_pc, vip->ih, ipl,
	    intrhand, intrarg, intr_xname);
	KASSERT(vip->cookie != NULL);

	return vip->cookie;
}

static void
vmd_intr_disestablish(void *ctx, pci_chipset_tag_t pc, void *ih)
{
//	struct vmd_softc * const sc = ctx;

	/* XXXRO not yet */
}

static pci_intr_type_t
vmd_intr_type(void *ctx, pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	struct vmd_softc * const sc = ctx;
	struct vmd_intr *vip;
	int i;

	DPRINTF("%s: sc_msix_count %d\n", __func__, sc->sc_msix_count);

	if (sc->sc_msix_count == 0)
		return pci_intr_type(pc, ih);

	i = VMD_IH_TO_INDEX(ih);
	vip = &sc->sc_intr[i];

	DPRINTF("%s: i %d type %d pa %p ih 0x%016lx\n", __func__,
	    i, (int)vip->type, vip->pa, ih);

	return vip->type;
}

static const char *
vmd_intr_string(void *ctx, pci_chipset_tag_t pc, pci_intr_handle_t ih,
    char *buf, size_t len)
{
	struct vmd_softc * const sc = ctx;
	struct vmd_intr *vip;
	char pci_buf[PCI_INTRSTR_LEN];
	const char *type;
	int i;

	if (sc->sc_msix_count == 0) {
 bypass:
		return pci_intr_string(pc, ih, buf, len);
	}

	i = VMD_IH_TO_INDEX(ih);
	vip = &sc->sc_intr[i];
	switch (vip->type) {
	case PCI_INTR_TYPE_MSIX:
		type = "msix";
		break;
	case PCI_INTR_TYPE_MSI:
		type = "msi";
		break;
	case PCI_INTR_TYPE_INTX:
		goto bypass;
	default:
		return NULL;
	}

	snprintf(buf, len, "%s %s vec %d (%s)", device_xname(sc->sc_dev),
	    type, VMD_INDEX_TO_VEC(sc, i),
	    pci_intr_string(sc->sc_pc, vip->ih, pci_buf, sizeof(pci_buf)));

	return buf;
}
