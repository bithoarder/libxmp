/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"


#define PTM_CH_MASK	0x1f
#define PTM_NI_FOLLOW	0x20
#define PTM_VOL_FOLLOWS	0x80
#define PTM_FX_FOLLOWS	0x40

struct ptm_file_header {
	uint8 name[28];		/* Song name */
	uint8 doseof;		/* 0x1a */
	uint8 vermin;		/* Minor version */
	uint8 vermaj;		/* Major type */
	uint8 rsvd1;		/* Reserved */
	uint16 ordnum;		/* Number of orders (must be even) */
	uint16 insnum;		/* Number of instruments */
	uint16 patnum;		/* Number of patterns */
	uint16 chnnum;		/* Number of channels */
	uint16 flags;		/* Flags (set to 0) */
	uint16 rsvd2;		/* Reserved */
	uint32 magic;		/* 'PTMF' */
	uint8 rsvd3[16];	/* Reserved */
	uint8 chset[32];	/* Channel settings */
	uint8 order[256];	/* Orders */
	uint16 patseg[128];
};

struct ptm_instrument_header {
	uint8 type;		/* Sample type */
	uint8 dosname[12];	/* DOS file name */
	uint8 vol;		/* Volume */
	uint16 c4spd;		/* C4 speed */
	uint16 smpseg;		/* Sample segment (not used) */
	uint32 smpofs;		/* Sample offset */
	uint32 length;		/* Length */
	uint32 loopbeg;		/* Loop begin */
	uint32 loopend;		/* Loop end */
	uint32 gusbeg;		/* GUS begin address */
	uint32 guslps;		/* GUS loop start address */
	uint32 guslpe;		/* GUS loop end address */
	uint8 gusflg;		/* GUS loop flags */
	uint8 rsvd1;		/* Reserved */
	uint8 name[28];		/* Instrument name */
	uint32 magic;		/* 'PTMS' */
};

#define MAGIC_PTMF	MAGIC4('P','T','M','F')


static int ptm_test (xmp_file, char *, const int);
static int ptm_load (struct module_data *, xmp_file, const int);

const struct format_loader ptm_loader = {
    "Poly Tracker (PTM)",
    ptm_test,
    ptm_load
};

static int ptm_test(xmp_file f, char *t, const int start)
{
    xmp_fseek(f, start + 44, SEEK_SET);
    if (read32b(f) != MAGIC_PTMF)
	return -1;

    xmp_fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}


static const int ptm_vol[] = {
     0,  5,  8, 10, 12, 14, 15, 17, 18, 20, 21, 22, 23, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40,
    41, 42, 42, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 51,
    52, 53, 54, 54, 55, 56, 56, 57, 58, 58, 59, 59, 60, 61, 61,
    62, 63, 63, 64, 64
};


static int ptm_load(struct module_data *m, xmp_file f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int c, r, i, smp_ofs[256];
    struct xmp_event *event;
    struct ptm_file_header pfh;
    struct ptm_instrument_header pih;
    uint8 n, b;

    LOAD_INIT();

    /* Load and convert header */

    xmp_fread(&pfh.name, 28, 1, f);		/* Song name */
    pfh.doseof = read8(f);		/* 0x1a */
    pfh.vermin = read8(f);		/* Minor version */
    pfh.vermaj = read8(f);		/* Major type */
    pfh.rsvd1 = read8(f);		/* Reserved */
    pfh.ordnum = read16l(f);		/* Number of orders (must be even) */
    pfh.insnum = read16l(f);		/* Number of instruments */
    pfh.patnum = read16l(f);		/* Number of patterns */
    pfh.chnnum = read16l(f);		/* Number of channels */
    pfh.flags = read16l(f);		/* Flags (set to 0) */
    pfh.rsvd2 = read16l(f);		/* Reserved */
    pfh.magic = read32b(f); 		/* 'PTMF' */

#if 0
    if (pfh.magic != MAGIC_PTMF)
	return -1;
#endif

    xmp_fread(&pfh.rsvd3, 16, 1, f);	/* Reserved */
    xmp_fread(&pfh.chset, 32, 1, f);	/* Channel settings */
    xmp_fread(&pfh.order, 256, 1, f);	/* Orders */
    for (i = 0; i < 128; i++)
	pfh.patseg[i] = read16l(f);

    mod->len = pfh.ordnum;
    mod->ins = pfh.insnum;
    mod->pat = pfh.patnum;
    mod->chn = pfh.chnnum;
    mod->trk = mod->pat * mod->chn;
    mod->smp = mod->ins;
    mod->spd = 6;
    mod->bpm = 125;
    memcpy (mod->xxo, pfh.order, 256);

    m->c4rate = C4_NTSC_RATE;

    copy_adjust(mod->name, pfh.name, 28);
    set_type(m, "Poly Tracker PTM %d.%02x",
	pfh.vermaj, pfh.vermin);

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	pih.type = read8(f);			/* Sample type */
	xmp_fread(&pih.dosname, 12, 1, f);		/* DOS file name */
	pih.vol = read8(f);			/* Volume */
	pih.c4spd = read16l(f);			/* C4 speed */
	pih.smpseg = read16l(f);		/* Sample segment (not used) */
	pih.smpofs = read32l(f);		/* Sample offset */
	pih.length = read32l(f);		/* Length */
	pih.loopbeg = read32l(f);		/* Loop begin */
	pih.loopend = read32l(f);		/* Loop end */
	pih.gusbeg = read32l(f);		/* GUS begin address */
	pih.guslps = read32l(f);		/* GUS loop start address */
	pih.guslpe = read32l(f);		/* GUS loop end address */
	pih.gusflg = read8(f);			/* GUS loop flags */
	pih.rsvd1 = read8(f);			/* Reserved */
	xmp_fread(&pih.name, 28, 1, f);		/* Instrument name */
	pih.magic = read32b(f);			/* 'PTMS' */

	if ((pih.type & 3) != 1)
	    continue;

	smp_ofs[i] = pih.smpofs;
	mod->xxs[i].len = pih.length;
	mod->xxi[i].nsm = pih.length > 0 ? 1 : 0;
	mod->xxs[i].lps = pih.loopbeg;
	mod->xxs[i].lpe = pih.loopend;

	mod->xxs[i].flg = 0;
	if (pih.type & 0x04) {
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP;
	}
	if (pih.type & 0x08) {
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR;
	}
	if (pih.type & 0x10) {
	    mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
	    mod->xxs[i].len >>= 1;
	    mod->xxs[i].lps >>= 1;
	    mod->xxs[i].lpe >>= 1;
	}

	mod->xxi[i].sub[0].vol = pih.vol;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;
	pih.magic = 0;

	copy_adjust(mod->xxi[i].name, pih.name, 28);

	D_(D_INFO "[%2X] %-28.28s %05x%c%05x %05x %c V%02x %5d",
		i, mod->xxi[i].name, mod->xxs[i].len,
		pih.type & 0x10 ? '+' : ' ',
		mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, pih.c4spd);

	/* Convert C4SPD to relnote/finetune */
	c2spd_to_note(pih.c4spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
    }

    PATTERN_INIT();

    /* Read patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	if (!pfh.patseg[i])
	    continue;
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	xmp_fseek(f, start + 16L * pfh.patseg[i], SEEK_SET);
	r = 0;
	while (r < 64) {
	    b = read8(f);
	    if (!b) {
		r++;
		continue;
	    }

	    c = b & PTM_CH_MASK;
	    if (c >= mod->chn)
		continue;

	    event = &EVENT (i, c, r);
	    if (b & PTM_NI_FOLLOW) {
		n = read8(f);
		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = XMP_KEY_OFF;
		    break;	/* Key off */
		default:
		    n += 12;
		}
		event->note = n;
		event->ins = read8(f);
	    }
	    if (b & PTM_FX_FOLLOWS) {
		event->fxt = read8(f);
		event->fxp = read8(f);

		if (event->fxt > 0x17)
			event->fxt = event->fxp = 0;

		switch (event->fxt) {
		case 0x0e:	/* Extended effect */
		    if (MSN(event->fxp) == 0x8) {	/* Pan set */
			event->fxt = FX_SETPAN;
			event->fxp = LSN (event->fxp) << 4;
		    }
		    break;
		case 0x10:	/* Set global volume */
		    event->fxt = FX_GLOBALVOL;
		    break;
		case 0x11:	/* Multi retrig */
		    event->fxt = FX_MULTI_RETRIG;
		    break;
		case 0x12:	/* Fine vibrato */
		    event->fxt = FX_FINE_VIBRATO;
		    break;
		case 0x13:	/* Note slide down */
		    event->fxt = FX_NSLIDE_DN;
		    break;
		case 0x14:	/* Note slide up */
		    event->fxt = FX_NSLIDE_UP;
		    break;
		case 0x15:	/* Note slide down + retrig */
		    event->fxt = FX_NSLIDE_R_DN;
		    break;
		case 0x16:	/* Note slide up + retrig */
		    event->fxt = FX_NSLIDE_R_UP;
		    break;
		case 0x17:	/* Reverse sample */
		    event->fxt = event->fxp = 0;
		    break;
		}
	    }
	    if (b & PTM_VOL_FOLLOWS) {
		event->vol = read8(f) + 1;
	    }
	}
    }

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->smp; i++) {
	struct xmp_sample *xxs;
	int smpnum;

	if (mod->xxi[i].nsm == 0)
	    continue;

	smpnum = mod->xxi[i].sub[0].sid;
	xxs = &mod->xxs[smpnum];

	if (xxs->len == 0)
	    continue;

	xmp_fseek(f, start + smp_ofs[smpnum], SEEK_SET);
	load_sample(m, f, SAMPLE_FLAG_8BDIFF, xxs, NULL);
    }

    m->vol_table = (int *)ptm_vol;

    for (i = 0; i < mod->chn; i++)
	mod->xxc[i].pan = pfh.chset[i] << 4;

    return 0;
}
