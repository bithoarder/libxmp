/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* From the STMIK 0.2 documentation:
 *
 * "The STMIK uses a special [Scream Tracker] beta-V3.0 module format.
 *  Due to the module formats beta nature, the current STMIK uses a .STX
 *  extension instead of the normal .STM. I'm not intending to do a
 *  STX->STM converter, so treat STX as the format to be used in finished
 *  programs, NOT as a format to be used in distributing modules. A program
 *  called STM2STX is included, and it'll convert STM modules to the STX
 *  format for usage in your own programs."
 *
 * Tested using "Future Brain" from Mental Surgery by Future Crew and
 * STMs converted with STM2STX.
 */

#include "loader.h"
#include "s3m.h"
#include "period.h"


struct stx_file_header {
	uint8 name[20];		/* Song name */
	uint8 magic[8];		/* !Scream! */
	uint16 psize;		/* Pattern 0 size? */
	uint16 unknown1;	/* ??!? */
	uint16 pp_pat;		/* Pointer to pattern table */
	uint16 pp_ins;		/* Pattern to instrument table */
	uint16 pp_chn;		/* Pointer to channel table (?) */
	uint16 unknown2;
	uint16 unknown3;
	uint8 gvol;		/* Global volume */
	uint8 tempo;		/* Playback tempo */
	uint16 unknown4;
	uint16 unknown5;
	uint16 patnum;		/* Number of patterns */
	uint16 insnum;		/* Number of instruments */
	uint16 ordnum;		/* Number of orders */
	uint16 unknown6;	/* Flags? */
	uint16 unknown7;	/* Version? */
	uint16 unknown8;	/* Ffi? */
	uint8 magic2[4];	/* 'SCRM' */
};

struct stx_instrument_header {
	uint8 type;		/* Instrument type */
	uint8 dosname[13];	/* DOS file name */
	uint16 memseg;		/* Pointer to sample data */
	uint32 length;		/* Length */
	uint32 loopbeg;		/* Loop begin */
	uint32 loopend;		/* Loop end */
	uint8 vol;		/* Volume */
	uint8 rsvd1;		/* Reserved */
	uint8 pack;		/* Packing type (not used) */
	uint8 flags;		/* Loop/stereo/16bit samples flags */
	uint16 c2spd;		/* C 4 speed */
	uint16 rsvd2;		/* Reserved */
	uint8 rsvd3[4];		/* Reserved */
	uint16 int_gp;		/* Internal - GUS pointer */
	uint16 int_512;		/* Internal - SB pointer */
	uint32 int_last;	/* Internal - SB index */
	uint8 name[28];		/* Instrument name */
	uint8 magic[4];		/* Reserved (for 'SCRS') */
};


static int stx_test (xmp_file, char *, const int);
static int stx_load (struct module_data *, xmp_file, const int);

const struct format_loader stx_loader = {
    "STMIK 0.2 (STX)",
    stx_test,
    stx_load
};

static int stx_test(xmp_file f, char *t, const int start)
{
    char buf[8];

    xmp_fseek(f, start + 20, SEEK_SET);
    if (xmp_fread(buf, 1, 8, f) < 8)
	return -1;
    if (memcmp(buf, "!Scream!", 8) && memcmp(buf, "BMOD2STM", 8))
	return -1;

    xmp_fseek(f, start + 60, SEEK_SET);
    if (xmp_fread(buf, 1, 4, f) < 4)
	return -1;
    if (memcmp(buf, "SCRM", 4))
	return -1;

    xmp_fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 20);

    return 0;
}

#define FX_NONE 0xff

static const uint8 fx[] = {
    FX_NONE,		FX_SPEED,
    FX_JUMP,		FX_BREAK,
    FX_VOLSLIDE,	FX_PORTA_DN,
    FX_PORTA_UP,	FX_TONEPORTA,
    FX_VIBRATO,		FX_TREMOR,
    FX_ARPEGGIO
};


static int stx_load(struct module_data *m, xmp_file f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int c, r, i, broken = 0;
    struct xmp_event *event = 0, dummy;
    struct stx_file_header sfh;
    struct stx_instrument_header sih;
    uint8 n, b;
    uint16 x16;
    int bmod2stm = 0;
    uint16 *pp_ins;		/* Parapointers to instruments */
    uint16 *pp_pat;		/* Parapointers to patterns */

    LOAD_INIT();

    xmp_fread(&sfh.name, 20, 1, f);
    xmp_fread(&sfh.magic, 8, 1, f);
    sfh.psize = read16l(f);
    sfh.unknown1 = read16l(f);
    sfh.pp_pat = read16l(f);
    sfh.pp_ins = read16l(f);
    sfh.pp_chn = read16l(f);
    sfh.unknown2 = read16l(f);
    sfh.unknown3 = read16l(f);
    sfh.gvol = read8(f);
    sfh.tempo = read8(f);
    sfh.unknown4 = read16l(f);
    sfh.unknown5 = read16l(f);
    sfh.patnum = read16l(f);
    sfh.insnum = read16l(f);
    sfh.ordnum = read16l(f);
    sfh.unknown6 = read16l(f);
    sfh.unknown7 = read16l(f);
    sfh.unknown8 = read16l(f);
    xmp_fread(&sfh.magic2, 4, 1, f);

    /* BMOD2STM does not convert pitch */
    if (!strncmp ((char *) sfh.magic, "BMOD2STM", 8))
	bmod2stm = 1;

#if 0
    if ((strncmp ((char *) sfh.magic, "!Scream!", 8) &&
	!bmod2stm) || strncmp ((char *) sfh.magic2, "SCRM", 4))
	return -1;
#endif

    mod->ins = sfh.insnum;
    mod->pat = sfh.patnum;
    mod->trk = mod->pat * mod->chn;
    mod->len = sfh.ordnum;
    mod->spd = MSN (sfh.tempo);
    mod->smp = mod->ins;
    m->c4rate = C4_NTSC_RATE;

    /* STM2STX 1.0 released with STMIK 0.2 converts STMs with the pattern
     * length encoded in the first two bytes of the pattern (like S3M).
     */
    xmp_fseek(f, start + (sfh.pp_pat << 4), SEEK_SET);
    x16 = read16l(f);
    xmp_fseek(f, start + (x16 << 4), SEEK_SET);
    x16 = read16l(f);
    if (x16 == sfh.psize)
	broken = 1;

    strncpy(mod->name, (char *)sfh.name, 20);
    if (bmod2stm)
	set_type(m, "BMOD2STM STX");
    else
	snprintf(mod->type, XMP_NAME_SIZE, "STM2STX 1.%d", broken ? 0 : 1);

    MODULE_INFO();
 
    pp_pat = calloc (2, mod->pat);
    pp_ins = calloc (2, mod->ins);

    /* Read pattern pointers */
    xmp_fseek(f, start + (sfh.pp_pat << 4), SEEK_SET);
    for (i = 0; i < mod->pat; i++)
	pp_pat[i] = read16l(f);

    /* Read instrument pointers */
    xmp_fseek(f, start + (sfh.pp_ins << 4), SEEK_SET);
    for (i = 0; i < mod->ins; i++)
	pp_ins[i] = read16l(f);

    /* Skip channel table (?) */
    xmp_fseek(f, start + (sfh.pp_chn << 4) + 32, SEEK_SET);

    /* Read orders */
    for (i = 0; i < mod->len; i++) {
	mod->xxo[i] = read8(f);
	xmp_fseek(f, 4, SEEK_CUR);
    }
 
    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	xmp_fseek(f, start + (pp_ins[i] << 4), SEEK_SET);

	sih.type = read8(f);
	xmp_fread(&sih.dosname, 13, 1, f);
	sih.memseg = read16l(f);
	sih.length = read32l(f);
	sih.loopbeg = read32l(f);
	sih.loopend = read32l(f);
	sih.vol = read8(f);
	sih.rsvd1 = read8(f);
	sih.pack = read8(f);
	sih.flags = read8(f);
	sih.c2spd = read16l(f);
	sih.rsvd2 = read16l(f);
	xmp_fread(&sih.rsvd3, 4, 1, f);
	sih.int_gp = read16l(f);
	sih.int_512 = read16l(f);
	sih.int_last = read32l(f);
	xmp_fread(&sih.name, 28, 1, f);
	xmp_fread(&sih.magic, 4, 1, f);

	mod->xxi[i].nsm = !!(mod->xxs[i].len = sih.length);
	mod->xxs[i].lps = sih.loopbeg;
	mod->xxs[i].lpe = sih.loopend;
	if (mod->xxs[i].lpe == 0xffff)
	    mod->xxs[i].lpe = 0;
	mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = sih.vol;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, sih.name, 12);

	D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c V%02x %5d\n", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, sih.c2spd);

	sih.c2spd = 8363 * sih.c2spd / 8448;
	c2spd_to_note(sih.c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
    }

    PATTERN_INIT();

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (!pp_pat[i])
	    continue;

	xmp_fseek(f, start + (pp_pat[i] << 4), SEEK_SET);
	if (broken)
	    xmp_fseek(f, 2, SEEK_CUR);

	for (r = 0; r < 64; ) {
	    b = read8(f);

	    if (b == S3M_EOR) {
		r++;
		continue;
	    }

	    c = b & S3M_CH_MASK;
	    event = c >= mod->chn ? &dummy : &EVENT (i, c, r);

	    if (b & S3M_NI_FOLLOW) {
		n = read8(f);

		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = XMP_KEY_OFF;
		    break;	/* Key off */
		default:
		    n = 37 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		event->ins = read8(f);;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		event->vol = read8(f) + 1;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		event->fxt = fx[read8(f)];
		event->fxp = read8(f);
		switch (event->fxt) {
		case FX_SPEED:
		    event->fxp = MSN (event->fxp);
		    break;
		case FX_NONE:
		    event->fxp = event->fxt = 0;
		    break;
		}
	    }
	}

    }

    free (pp_pat);
    free (pp_ins);

    /* Read samples */
    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	load_sample(m, f, 0, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
    }

    m->quirk |= QUIRK_VSALL | QUIRKS_ST3;
    m->read_event_type = READ_EVENT_ST3;

    return 0;
}
