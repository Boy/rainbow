/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux and Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2007, Callum Lerwick <seg@haxxed.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opj_includes.h"
#include "t1_luts.h"

/** @defgroup T1 T1 - Implementation of the tier-1 coding */
/*@{*/

/** @name Local static functions */
/*@{*/

static INLINE char t1_getctxno_zc(S32 f, S32 orient);
static char t1_getctxno_sc(S32 f);
static INLINE S32 t1_getctxno_mag(S32 f);
static char t1_getspb(S32 f);
static S16 t1_getnmsedec_sig(S32 x, S32 bitpos);
static S16 t1_getnmsedec_ref(S32 x, S32 bitpos);
static void t1_updateflags(flag_t *flagsp, S32 s, S32 stride);
/**
Encode significant pass
*/
static void t1_enc_sigpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		char type,
		S32 vsc);
/**
Decode significant pass
*/
static INLINE void t1_dec_sigpass_step_raw(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 vsc);
static INLINE void t1_dec_sigpass_step_mqc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf);
static INLINE void t1_dec_sigpass_step_mqc_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 vsc);
/**
Encode significant pass
*/
static void t1_enc_sigpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 *nmsedec,
		char type,
		S32 cblksty);
/**
Decode significant pass
*/
static void t1_dec_sigpass_raw(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 cblksty);
static void t1_dec_sigpass_mqc(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient);
static void t1_dec_sigpass_mqc_vsc(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient);
/**
Encode refinement pass
*/
static void t1_enc_refpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		char type,
		S32 vsc);
/**
Decode refinement pass
*/
static void INLINE t1_dec_refpass_step_raw(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf,
		S32 vsc);
static void INLINE t1_dec_refpass_step_mqc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf);
static void INLINE t1_dec_refpass_step_mqc_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf,
		S32 vsc);

/**
Encode refinement pass
*/
static void t1_enc_refpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 *nmsedec,
		char type,
		S32 cblksty);
/**
Decode refinement pass
*/
static void t1_dec_refpass_raw(
		opj_t1_t *t1,
		S32 bpno,
		S32 cblksty);
static void t1_dec_refpass_mqc(
		opj_t1_t *t1,
		S32 bpno);
static void t1_dec_refpass_mqc_vsc(
		opj_t1_t *t1,
		S32 bpno);
/**
Encode clean-up pass
*/
static void t1_enc_clnpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		S32 partial,
		S32 vsc);
/**
Decode clean-up pass
*/
static void t1_dec_clnpass_step_partial(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf);
static void t1_dec_clnpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf);
static void t1_dec_clnpass_step_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 partial,
		S32 vsc);
/**
Encode clean-up pass
*/
static void t1_enc_clnpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 *nmsedec,
		S32 cblksty);
/**
Decode clean-up pass
*/
static void t1_dec_clnpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 cblksty);
static F64 t1_getwmsedec(
		S32 nmsedec,
		S32 compno,
		S32 level,
		S32 orient,
		S32 bpno,
		S32 qmfbid,
		F64 stepsize,
		S32 numcomps,
		S32 mct);
/**
Encode 1 code-block
@param t1 T1 handle
@param cblk Code-block coding parameters
@param orient
@param compno Component number
@param level
@param qmfbid
@param stepsize
@param cblksty Code-block style
@param numcomps
@param mct
@param tile
*/
static void t1_encode_cblk(
		opj_t1_t *t1,
		opj_tcd_cblk_enc_t* cblk,
		S32 orient,
		S32 compno,
		S32 level,
		S32 qmfbid,
		F64 stepsize,
		S32 cblksty,
		S32 numcomps,
		S32 mct,
		opj_tcd_tile_t * tile);
/**
Decode 1 code-block
@param t1 T1 handle
@param cblk Code-block coding parameters
@param orient
@param roishift Region of interest shifting value
@param cblksty Code-block style
*/
static void t1_decode_cblk(
		opj_t1_t *t1,
		opj_tcd_cblk_dec_t* cblk,
		S32 orient,
		S32 roishift,
		S32 cblksty);

/*@}*/

/*@}*/

/* ----------------------------------------------------------------------- */

static char t1_getctxno_zc(S32 f, S32 orient) {
	return lut_ctxno_zc[(orient << 8) | (f & T1_SIG_OTH)];
}

static char t1_getctxno_sc(S32 f) {
	return lut_ctxno_sc[(f & (T1_SIG_PRIM | T1_SGN)) >> 4];
}

static S32 t1_getctxno_mag(S32 f) {
	S32 tmp1 = (f & T1_SIG_OTH) ? T1_CTXNO_MAG + 1 : T1_CTXNO_MAG;
	S32 tmp2 = (f & T1_REFINE) ? T1_CTXNO_MAG + 2 : tmp1;
	return (tmp2);
}

static char t1_getspb(S32 f) {
	return lut_spb[(f & (T1_SIG_PRIM | T1_SGN)) >> 4];
}

static short t1_getnmsedec_sig(S32 x, S32 bitpos) {
	if (bitpos > T1_NMSEDEC_FRACBITS) {
		return lut_nmsedec_sig[(x >> (bitpos - T1_NMSEDEC_FRACBITS)) & ((1 << T1_NMSEDEC_BITS) - 1)];
	}
	
	return lut_nmsedec_sig0[x & ((1 << T1_NMSEDEC_BITS) - 1)];
}

static short t1_getnmsedec_ref(S32 x, S32 bitpos) {
	if (bitpos > T1_NMSEDEC_FRACBITS) {
		return lut_nmsedec_ref[(x >> (bitpos - T1_NMSEDEC_FRACBITS)) & ((1 << T1_NMSEDEC_BITS) - 1)];
	}

    return lut_nmsedec_ref0[x & ((1 << T1_NMSEDEC_BITS) - 1)];
}

static void t1_updateflags(flag_t *flagsp, S32 s, S32 stride) {
	flag_t *np = flagsp - stride;
	flag_t *sp = flagsp + stride;

	static const flag_t mod[] = {
		T1_SIG_S, T1_SIG_S|T1_SGN_S,
		T1_SIG_E, T1_SIG_E|T1_SGN_E,
		T1_SIG_W, T1_SIG_W|T1_SGN_W,
		T1_SIG_N, T1_SIG_N|T1_SGN_N
	};

	np[-1] |= T1_SIG_SE;
	np[0]  |= mod[s];
	np[1]  |= T1_SIG_SW;

	flagsp[-1] |= mod[s+2];
	flagsp[0]  |= T1_SIG;
	flagsp[1]  |= mod[s+4];

	sp[-1] |= T1_SIG_NE;
	sp[0]  |= mod[s+6];
	sp[1]  |= T1_SIG_NW;
}

static void t1_enc_sigpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		char type,
		S32 vsc)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & T1_SIG_OTH) && !(flag & (T1_SIG | T1_VISIT))) {
		v = int_abs(*datap) & one ? 1 : 0;
		mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));	/* ESSAI */
		if (type == T1_TYPE_RAW) {	/* BYPASS/LAZY MODE */
			mqc_bypass_enc(mqc, v);
		} else {
			mqc_encode(mqc, v);
		}
		if (v) {
			v = *datap < 0 ? 1 : 0;
			*nmsedec +=	t1_getnmsedec_sig(int_abs(*datap), bpno + T1_NMSEDEC_FRACBITS);
			mqc_setcurctx(mqc, t1_getctxno_sc(flag));	/* ESSAI */
			if (type == T1_TYPE_RAW) {	/* BYPASS/LAZY MODE */
				mqc_bypass_enc(mqc, v);
			} else {
				mqc_encode(mqc, v ^ t1_getspb(flag));
			}
			t1_updateflags(flagsp, v, t1->flags_stride);
		}
		*flagsp |= T1_VISIT;
	}
}

static INLINE void t1_dec_sigpass_step_raw(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 vsc)
{
	S32 v, flag;
	
	opj_raw_t *raw = t1->raw;	/* RAW component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & T1_SIG_OTH) && !(flag & (T1_SIG | T1_VISIT))) {
			if (raw_decode(raw)) {
				v = raw_decode(raw);	/* ESSAI */
				*datap = v ? -oneplushalf : oneplushalf;
				t1_updateflags(flagsp, v, t1->flags_stride);
			}
		*flagsp |= T1_VISIT;
	}
}				/* VSC and  BYPASS by Antonin */

static INLINE void t1_dec_sigpass_step_mqc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = *flagsp;
	if ((flag & T1_SIG_OTH) && !(flag & (T1_SIG | T1_VISIT))) {
			mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));
			if (mqc_decode(mqc)) {
				mqc_setcurctx(mqc, t1_getctxno_sc(flag));
				v = mqc_decode(mqc) ^ t1_getspb(flag);
				*datap = v ? -oneplushalf : oneplushalf;
				t1_updateflags(flagsp, v, t1->flags_stride);
			}
		*flagsp |= T1_VISIT;
	}
}				/* VSC and  BYPASS by Antonin */

static INLINE void t1_dec_sigpass_step_mqc_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 vsc)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & T1_SIG_OTH) && !(flag & (T1_SIG | T1_VISIT))) {
		mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));
		if (mqc_decode(mqc)) {
			mqc_setcurctx(mqc, t1_getctxno_sc(flag));
			v = mqc_decode(mqc) ^ t1_getspb(flag);
			*datap = v ? -oneplushalf : oneplushalf;
			t1_updateflags(flagsp, v, t1->flags_stride);
		}
		*flagsp |= T1_VISIT;
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_enc_sigpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 *nmsedec,
		char type,
		S32 cblksty)
{
	S32 i, j, k, one, vsc;
	*nmsedec = 0;
	one = 1 << (bpno + T1_NMSEDEC_FRACBITS);
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = ((cblksty & J2K_CCP_CBLKSTY_VSC) && (j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_enc_sigpass_step(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						orient,
						bpno,
						one,
						nmsedec,
						type,
						vsc);
			}
		}
	}
}

static void t1_dec_sigpass_raw(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 cblksty)
{
	S32 i, j, k, one, half, oneplushalf, vsc;
	one = 1 << bpno;
	half = one >> 1;
	oneplushalf = one | half;
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = ((cblksty & J2K_CCP_CBLKSTY_VSC) && (j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_dec_sigpass_step_raw(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						orient,
						oneplushalf,
						vsc);
			}
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_sigpass_mqc(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient)
{
	S32 i, j, k, one, half, oneplushalf;
	S32 *data1 = t1->data;
	flag_t *flags1 = &t1->flags[1];
	one = 1 << bpno;
	half = one >> 1;
	oneplushalf = one | half;
	for (k = 0; k < (t1->h & ~3); k += 4) {
		for (i = 0; i < t1->w; ++i) {
			S32 *data2 = data1 + i;
			flag_t *flags2 = flags1 + i;
			flags2 += t1->flags_stride;
			t1_dec_sigpass_step_mqc(t1, flags2, data2, orient, oneplushalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_sigpass_step_mqc(t1, flags2, data2, orient, oneplushalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_sigpass_step_mqc(t1, flags2, data2, orient, oneplushalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_sigpass_step_mqc(t1, flags2, data2, orient, oneplushalf);
			data2 += t1->w;
		}
		data1 += t1->w << 2;
		flags1 += t1->flags_stride << 2;
	}
	for (i = 0; i < t1->w; ++i) {
		S32 *data2 = data1 + i;
		flag_t *flags2 = flags1 + i;
		for (j = k; j < t1->h; ++j) {
			flags2 += t1->flags_stride;
			t1_dec_sigpass_step_mqc(t1, flags2, data2, orient, oneplushalf);
			data2 += t1->w;
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_sigpass_mqc_vsc(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient)
{
	S32 i, j, k, one, half, oneplushalf, vsc;
	one = 1 << bpno;
	half = one >> 1;
	oneplushalf = one | half;
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = (j == k + 3 || j == t1->h - 1) ? 1 : 0;
				t1_dec_sigpass_step_mqc_vsc(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						orient,
						oneplushalf,
						vsc);
			}
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_enc_refpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		char type,
		S32 vsc)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & (T1_SIG | T1_VISIT)) == T1_SIG) {
		*nmsedec += t1_getnmsedec_ref(int_abs(*datap), bpno + T1_NMSEDEC_FRACBITS);
		v = int_abs(*datap) & one ? 1 : 0;
		mqc_setcurctx(mqc, t1_getctxno_mag(flag));	/* ESSAI */
		if (type == T1_TYPE_RAW) {	/* BYPASS/LAZY MODE */
			mqc_bypass_enc(mqc, v);
		} else {
			mqc_encode(mqc, v);
		}
		*flagsp |= T1_REFINE;
	}
}

static INLINE void t1_dec_refpass_step_raw(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf,
		S32 vsc)
{
	S32 v, t, flag;
	
	opj_raw_t *raw = t1->raw;	/* RAW component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & (T1_SIG | T1_VISIT)) == T1_SIG) {
			v = raw_decode(raw);
		t = v ? poshalf : neghalf;
		*datap += *datap < 0 ? -t : t;
		*flagsp |= T1_REFINE;
	}
}				/* VSC and  BYPASS by Antonin  */

static INLINE void t1_dec_refpass_step_mqc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf)
{
	S32 v, t, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = *flagsp;
	if ((flag & (T1_SIG | T1_VISIT)) == T1_SIG) {
		mqc_setcurctx(mqc, t1_getctxno_mag(flag));	/* ESSAI */
			v = mqc_decode(mqc);
		t = v ? poshalf : neghalf;
		*datap += *datap < 0 ? -t : t;
		*flagsp |= T1_REFINE;
		}
}				/* VSC and  BYPASS by Antonin  */

static INLINE void t1_dec_refpass_step_mqc_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 poshalf,
		S32 neghalf,
		S32 vsc)
{
	S32 v, t, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if ((flag & (T1_SIG | T1_VISIT)) == T1_SIG) {
		mqc_setcurctx(mqc, t1_getctxno_mag(flag));	/* ESSAI */
		v = mqc_decode(mqc);
		t = v ? poshalf : neghalf;
		*datap += *datap < 0 ? -t : t;
		*flagsp |= T1_REFINE;
	}
}				/* VSC and  BYPASS by Antonin  */

static void t1_enc_refpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 *nmsedec,
		char type,
		S32 cblksty)
{
	S32 i, j, k, one, vsc;
	*nmsedec = 0;
	one = 1 << (bpno + T1_NMSEDEC_FRACBITS);
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = ((cblksty & J2K_CCP_CBLKSTY_VSC) && (j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_enc_refpass_step(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						bpno,
						one,
						nmsedec,
						type,
						vsc);
			}
		}
	}
}

static void t1_dec_refpass_raw(
		opj_t1_t *t1,
		S32 bpno,
		S32 cblksty)
{
	S32 i, j, k, one, poshalf, neghalf;
	S32 vsc;
	one = 1 << bpno;
	poshalf = one >> 1;
	neghalf = bpno > 0 ? -poshalf : -1;
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = ((cblksty & J2K_CCP_CBLKSTY_VSC) && (j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_dec_refpass_step_raw(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						poshalf,
						neghalf,
						vsc);
			}
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_refpass_mqc(
		opj_t1_t *t1,
		S32 bpno)
{
	S32 i, j, k, one, poshalf, neghalf;
	S32 *data1 = t1->data;
	flag_t *flags1 = &t1->flags[1];
	one = 1 << bpno;
	poshalf = one >> 1;
	neghalf = bpno > 0 ? -poshalf : -1;
	for (k = 0; k < (t1->h & ~3); k += 4) {
		for (i = 0; i < t1->w; ++i) {
			S32 *data2 = data1 + i;
			flag_t *flags2 = flags1 + i;
			flags2 += t1->flags_stride;
			t1_dec_refpass_step_mqc(t1, flags2, data2, poshalf, neghalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_refpass_step_mqc(t1, flags2, data2, poshalf, neghalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_refpass_step_mqc(t1, flags2, data2, poshalf, neghalf);
			data2 += t1->w;
			flags2 += t1->flags_stride;
			t1_dec_refpass_step_mqc(t1, flags2, data2, poshalf, neghalf);
			data2 += t1->w;
		}
		data1 += t1->w << 2;
		flags1 += t1->flags_stride << 2;
	}
	for (i = 0; i < t1->w; ++i) {
		S32 *data2 = data1 + i;
		flag_t *flags2 = flags1 + i;
		for (j = k; j < t1->h; ++j) {
			flags2 += t1->flags_stride;
			t1_dec_refpass_step_mqc(t1, flags2, data2, poshalf, neghalf);
			data2 += t1->w;
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_refpass_mqc_vsc(
		opj_t1_t *t1,
		S32 bpno)
{
	S32 i, j, k, one, poshalf, neghalf;
	S32 vsc;
	one = 1 << bpno;
	poshalf = one >> 1;
	neghalf = bpno > 0 ? -poshalf : -1;
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			for (j = k; j < k + 4 && j < t1->h; ++j) {
				vsc = ((j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_dec_refpass_step_mqc_vsc(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						poshalf,
						neghalf,
						vsc);
			}
		}
	}
}				/* VSC and  BYPASS by Antonin */

static void t1_enc_clnpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 bpno,
		S32 one,
		S32 *nmsedec,
		S32 partial,
		S32 vsc)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if (partial) {
		goto LABEL_PARTIAL;
	}
	if (!(*flagsp & (T1_SIG | T1_VISIT))) {
		mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));
		v = int_abs(*datap) & one ? 1 : 0;
		mqc_encode(mqc, v);
		if (v) {
LABEL_PARTIAL:
			*nmsedec += t1_getnmsedec_sig(int_abs(*datap), bpno + T1_NMSEDEC_FRACBITS);
			mqc_setcurctx(mqc, t1_getctxno_sc(flag));
			v = *datap < 0 ? 1 : 0;
			mqc_encode(mqc, v ^ t1_getspb(flag));
			t1_updateflags(flagsp, v, t1->flags_stride);
		}
	}
	*flagsp &= ~T1_VISIT;
}

static void t1_dec_clnpass_step_partial(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = *flagsp;
	mqc_setcurctx(mqc, t1_getctxno_sc(flag));
	v = mqc_decode(mqc) ^ t1_getspb(flag);
	*datap = v ? -oneplushalf : oneplushalf;
	t1_updateflags(flagsp, v, t1->flags_stride);
	*flagsp &= ~T1_VISIT;
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_clnpass_step(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = *flagsp;
	if (!(flag & (T1_SIG | T1_VISIT))) {
		mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));
		if (mqc_decode(mqc)) {
			mqc_setcurctx(mqc, t1_getctxno_sc(flag));
			v = mqc_decode(mqc) ^ t1_getspb(flag);
			*datap = v ? -oneplushalf : oneplushalf;
			t1_updateflags(flagsp, v, t1->flags_stride);
		}
	}
	*flagsp &= ~T1_VISIT;
}				/* VSC and  BYPASS by Antonin */

static void t1_dec_clnpass_step_vsc(
		opj_t1_t *t1,
		flag_t *flagsp,
		S32 *datap,
		S32 orient,
		S32 oneplushalf,
		S32 partial,
		S32 vsc)
{
	S32 v, flag;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	flag = vsc ? ((*flagsp) & (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW | T1_SGN_S))) : (*flagsp);
	if (partial) {
		goto LABEL_PARTIAL;
	}
	if (!(flag & (T1_SIG | T1_VISIT))) {
		mqc_setcurctx(mqc, t1_getctxno_zc(flag, orient));
		if (mqc_decode(mqc)) {
LABEL_PARTIAL:
			mqc_setcurctx(mqc, t1_getctxno_sc(flag));
			v = mqc_decode(mqc) ^ t1_getspb(flag);
			*datap = v ? -oneplushalf : oneplushalf;
			t1_updateflags(flagsp, v, t1->flags_stride);
		}
	}
	*flagsp &= ~T1_VISIT;
}

static void t1_enc_clnpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 *nmsedec,
		S32 cblksty)
{
	S32 i, j, k, one, agg, runlen, vsc;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	*nmsedec = 0;
	one = 1 << (bpno + T1_NMSEDEC_FRACBITS);
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			if (k + 3 < t1->h) {
				if (cblksty & J2K_CCP_CBLKSTY_VSC) {
					agg = !(MACRO_t1_flags(1 + k,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 1,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 2,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| (MACRO_t1_flags(1 + k + 3,1 + i) 
						& (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW |	T1_SGN_S))) & (T1_SIG | T1_VISIT | T1_SIG_OTH));
				} else {
					agg = !(MACRO_t1_flags(1 + k,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 1,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 2,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 3,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH));
				}
			} else {
				agg = 0;
			}
			if (agg) {
				for (runlen = 0; runlen < 4; ++runlen) {
					if (int_abs(t1->data[((k + runlen)*t1->w) + i]) & one)
						break;
				}
				mqc_setcurctx(mqc, T1_CTXNO_AGG);
				mqc_encode(mqc, runlen != 4);
				if (runlen == 4) {
					continue;
				}
				mqc_setcurctx(mqc, T1_CTXNO_UNI);
				mqc_encode(mqc, runlen >> 1);
				mqc_encode(mqc, runlen & 1);
			} else {
				runlen = 0;
			}
			for (j = k + runlen; j < k + 4 && j < t1->h; ++j) {
				vsc = ((cblksty & J2K_CCP_CBLKSTY_VSC) && (j == k + 3 || j == t1->h - 1)) ? 1 : 0;
				t1_enc_clnpass_step(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						orient,
						bpno,
						one,
						nmsedec,
						agg && (j == k + runlen),
						vsc);
			}
		}
	}
}

static void t1_dec_clnpass(
		opj_t1_t *t1,
		S32 bpno,
		S32 orient,
		S32 cblksty)
{
	S32 i, j, k, one, half, oneplushalf, agg, runlen, vsc;
	S32 segsym = cblksty & J2K_CCP_CBLKSTY_SEGSYM;
	
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */
	
	one = 1 << bpno;
	half = one >> 1;
	oneplushalf = one | half;
	if (cblksty & J2K_CCP_CBLKSTY_VSC) {
	for (k = 0; k < t1->h; k += 4) {
		for (i = 0; i < t1->w; ++i) {
			if (k + 3 < t1->h) {
					agg = !(MACRO_t1_flags(1 + k,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 1,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| MACRO_t1_flags(1 + k + 2,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
						|| (MACRO_t1_flags(1 + k + 3,1 + i) 
						& (~(T1_SIG_S | T1_SIG_SE | T1_SIG_SW |	T1_SGN_S))) & (T1_SIG | T1_VISIT | T1_SIG_OTH));
				} else {
				agg = 0;
			}
			if (agg) {
				mqc_setcurctx(mqc, T1_CTXNO_AGG);
				if (!mqc_decode(mqc)) {
					continue;
				}
				mqc_setcurctx(mqc, T1_CTXNO_UNI);
				runlen = mqc_decode(mqc);
				runlen = (runlen << 1) | mqc_decode(mqc);
			} else {
				runlen = 0;
			}
			for (j = k + runlen; j < k + 4 && j < t1->h; ++j) {
					vsc = (j == k + 3 || j == t1->h - 1) ? 1 : 0;
					t1_dec_clnpass_step_vsc(
						t1,
						&t1->flags[((j+1) * t1->flags_stride) + i + 1],
						&t1->data[(j * t1->w) + i],
						orient,
						oneplushalf,
						agg && (j == k + runlen),
						vsc);
			}
		}
	}
	} else {
		S32 *data1 = t1->data;
		flag_t *flags1 = &t1->flags[1];
		for (k = 0; k < (t1->h & ~3); k += 4) {
			for (i = 0; i < t1->w; ++i) {
				S32 *data2 = data1 + i;
				flag_t *flags2 = flags1 + i;
				agg = !(MACRO_t1_flags(1 + k,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
					|| MACRO_t1_flags(1 + k + 1,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
					|| MACRO_t1_flags(1 + k + 2,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH)
					|| MACRO_t1_flags(1 + k + 3,1 + i) & (T1_SIG | T1_VISIT | T1_SIG_OTH));
				if (agg) {
					mqc_setcurctx(mqc, T1_CTXNO_AGG);
					if (!mqc_decode(mqc)) {
						continue;
					}
					mqc_setcurctx(mqc, T1_CTXNO_UNI);
					runlen = mqc_decode(mqc);
					runlen = (runlen << 1) | mqc_decode(mqc);
					flags2 += runlen * t1->flags_stride;
					data2 += runlen * t1->w;
					for (j = k + runlen; j < k + 4 && j < t1->h; ++j) {
						flags2 += t1->flags_stride;
						if (agg && (j == k + runlen)) {
							t1_dec_clnpass_step_partial(t1, flags2, data2, orient, oneplushalf);
						} else {
							t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
						}
						data2 += t1->w;
					}
				} else {
					flags2 += t1->flags_stride;
					t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
					data2 += t1->w;
					flags2 += t1->flags_stride;
					t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
					data2 += t1->w;
					flags2 += t1->flags_stride;
					t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
					data2 += t1->w;
					flags2 += t1->flags_stride;
					t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
					data2 += t1->w;
				}
			}
			data1 += t1->w << 2;
			flags1 += t1->flags_stride << 2;
		}
		for (i = 0; i < t1->w; ++i) {
			S32 *data2 = data1 + i;
			flag_t *flags2 = flags1 + i;
			for (j = k; j < t1->h; ++j) {
				flags2 += t1->flags_stride;
				t1_dec_clnpass_step(t1, flags2, data2, orient, oneplushalf);
				data2 += t1->w;
			}
		}
	}

	if (segsym) {
		S32 v = 0;
		mqc_setcurctx(mqc, T1_CTXNO_UNI);
		v = mqc_decode(mqc);
		v = (v << 1) | mqc_decode(mqc);
		v = (v << 1) | mqc_decode(mqc);
		v = (v << 1) | mqc_decode(mqc);
		/*
		if (v!=0xa) {
			opj_event_msg(t1->cinfo, EVT_WARNING, "Bad segmentation symbol %x\n", v);
		} 
		*/
	}
}				/* VSC and  BYPASS by Antonin */


/** mod fixed_quality */
static F64 t1_getwmsedec(
		S32 nmsedec,
		S32 compno,
		S32 level,
		S32 orient,
		S32 bpno,
		S32 qmfbid,
		F64 stepsize,
		S32 numcomps,
		S32 mct)
{
	F64 w1, w2, wmsedec;
	if (qmfbid == 1) {
		w1 = (mct && numcomps==3) ? mct_getnorm(compno) : 1.0;
		w2 = dwt_getnorm(level, orient);
	} else {			/* if (qmfbid == 0) */
		w1 = (mct && numcomps==3) ? mct_getnorm_real(compno) : 1.0;
		w2 = dwt_getnorm_real(level, orient);
	}
	wmsedec = w1 * w2 * stepsize * (1 << bpno);
	wmsedec *= wmsedec * nmsedec / 8192.0;
	
	return wmsedec;
}

static bool allocate_buffers(
		opj_t1_t *t1,
		S32 w,
		S32 h)
{
	S32 datasize=w * h;
	S32 flagssize;

	if(datasize > t1->datasize){
		opj_aligned_free(t1->data);
		t1->data = (S32*) opj_aligned_malloc(datasize * sizeof(S32));
		if(!t1->data){
			return false;
		}
		t1->datasize=datasize;
	}
	memset(t1->data,0,datasize * sizeof(S32));

	t1->flags_stride=w+2;
	flagssize=t1->flags_stride * (h+2);

	if(flagssize > t1->flagssize){
		opj_aligned_free(t1->flags);
		t1->flags = (flag_t*) opj_aligned_malloc(flagssize * sizeof(flag_t));
		if(!t1->flags){
			return false;
		}
		t1->flagssize=flagssize;
	}
	memset(t1->flags,0,flagssize * sizeof(flag_t));

	t1->w=w;
	t1->h=h;

	return true;
}

/** mod fixed_quality */
static void t1_encode_cblk(
		opj_t1_t *t1,
		opj_tcd_cblk_enc_t* cblk,
		S32 orient,
		S32 compno,
		S32 level,
		S32 qmfbid,
		F64 stepsize,
		S32 cblksty,
		S32 numcomps,
		S32 mct,
		opj_tcd_tile_t * tile)
{
	F64 cumwmsedec = 0.0;

	opj_mqc_t *mqc = t1->mqc;	/* MQC component */

	S32 passno, bpno, passtype;
	S32 nmsedec = 0;
	S32 i, max;
	char type = T1_TYPE_MQ;
	F64 tempwmsedec;

	max = 0;
	for (i = 0; i < t1->w * t1->h; ++i) {
		S32 tmp = abs(t1->data[i]);
		max = int_max(max, tmp);
	}

	cblk->numbps = max ? (int_floorlog2(max) + 1) - T1_NMSEDEC_FRACBITS : 0;
	
	bpno = cblk->numbps - 1;
	passtype = 2;
	
	mqc_resetstates(mqc);
	mqc_setstate(mqc, T1_CTXNO_UNI, 0, 46);
	mqc_setstate(mqc, T1_CTXNO_AGG, 0, 3);
	mqc_setstate(mqc, T1_CTXNO_ZC, 0, 4);
	mqc_init_enc(mqc, cblk->data);
	
	for (passno = 0; bpno >= 0; ++passno) {
		opj_tcd_pass_t *pass = &cblk->passes[passno];
		S32 correction = 3;
		type = ((bpno < (cblk->numbps - 4)) && (passtype < 2) && (cblksty & J2K_CCP_CBLKSTY_LAZY)) ? T1_TYPE_RAW : T1_TYPE_MQ;
		
		switch (passtype) {
			case 0:
				t1_enc_sigpass(t1, bpno, orient, &nmsedec, type, cblksty);
				break;
			case 1:
				t1_enc_refpass(t1, bpno, &nmsedec, type, cblksty);
				break;
			case 2:
				t1_enc_clnpass(t1, bpno, orient, &nmsedec, cblksty);
				/* code switch SEGMARK (i.e. SEGSYM) */
				if (cblksty & J2K_CCP_CBLKSTY_SEGSYM)
					mqc_segmark_enc(mqc);
				break;
		}
		
		/* fixed_quality */
		tempwmsedec = t1_getwmsedec(nmsedec, compno, level, orient, bpno, qmfbid, stepsize, numcomps, mct);
		cumwmsedec += tempwmsedec;
		tile->distotile += tempwmsedec;
		
		/* Code switch "RESTART" (i.e. TERMALL) */
		if ((cblksty & J2K_CCP_CBLKSTY_TERMALL)	&& !((passtype == 2) && (bpno - 1 < 0))) {
			if (type == T1_TYPE_RAW) {
				mqc_flush(mqc);
				correction = 1;
				/* correction = mqc_bypass_flush_enc(); */
			} else {			/* correction = mqc_restart_enc(); */
				mqc_flush(mqc);
				correction = 1;
			}
			pass->term = 1;
		} else {
			if (((bpno < (cblk->numbps - 4) && (passtype > 0)) 
				|| ((bpno == (cblk->numbps - 4)) && (passtype == 2))) && (cblksty & J2K_CCP_CBLKSTY_LAZY)) {
				if (type == T1_TYPE_RAW) {
					mqc_flush(mqc);
					correction = 1;
					/* correction = mqc_bypass_flush_enc(); */
				} else {		/* correction = mqc_restart_enc(); */
					mqc_flush(mqc);
					correction = 1;
				}
				pass->term = 1;
			} else {
				pass->term = 0;
			}
		}
		
		if (++passtype == 3) {
			passtype = 0;
			bpno--;
		}
		
		if (pass->term && bpno > 0) {
			type = ((bpno < (cblk->numbps - 4)) && (passtype < 2) && (cblksty & J2K_CCP_CBLKSTY_LAZY)) ? T1_TYPE_RAW : T1_TYPE_MQ;
			if (type == T1_TYPE_RAW)
				mqc_bypass_init_enc(mqc);
			else
				mqc_restart_init_enc(mqc);
		}
		
		pass->distortiondec = cumwmsedec;
		pass->rate = mqc_numbytes(mqc) + correction;	/* FIXME */
		
		/* Code-switch "RESET" */
		if (cblksty & J2K_CCP_CBLKSTY_RESET)
			mqc_reset_enc(mqc);
	}
	
	/* Code switch "ERTERM" (i.e. PTERM) */
	if (cblksty & J2K_CCP_CBLKSTY_PTERM)
		mqc_erterm_enc(mqc);
	else /* Default coding */ if (!(cblksty & J2K_CCP_CBLKSTY_LAZY))
		mqc_flush(mqc);
	
	cblk->totalpasses = passno;

	for (passno = 0; passno<cblk->totalpasses; passno++) {
		opj_tcd_pass_t *pass = &cblk->passes[passno];
		if (pass->rate > mqc_numbytes(mqc))
			pass->rate = mqc_numbytes(mqc);
		/*Preventing generation of FF as last data byte of a pass*/
		if((pass->rate>1) && (cblk->data[pass->rate - 1] == 0xFF)){
			pass->rate--;
		}
		pass->len = pass->rate - (passno == 0 ? 0 : cblk->passes[passno - 1].rate);		
	}
}

static void t1_decode_cblk(
		opj_t1_t *t1,
		opj_tcd_cblk_dec_t* cblk,
		S32 orient,
		S32 roishift,
		S32 cblksty)
{
	opj_raw_t *raw = t1->raw;	/* RAW component */
	opj_mqc_t *mqc = t1->mqc;	/* MQC component */

	S32 bpno, passtype;
	S32 segno, passno;
	char type = T1_TYPE_MQ; /* BYPASS mode */

	if(!allocate_buffers(
				t1,
				cblk->x1 - cblk->x0,
				cblk->y1 - cblk->y0))
	{
		return;
	}

	bpno = roishift + cblk->numbps - 1;
	passtype = 2;
	
	mqc_resetstates(mqc);
	mqc_setstate(mqc, T1_CTXNO_UNI, 0, 46);
	mqc_setstate(mqc, T1_CTXNO_AGG, 0, 3);
	mqc_setstate(mqc, T1_CTXNO_ZC, 0, 4);
	
	for (segno = 0; segno < cblk->numsegs; ++segno) {
		opj_tcd_seg_t *seg = &cblk->segs[segno];
		
		/* BYPASS mode */
		type = ((bpno <= (cblk->numbps - 1) - 4) && (passtype < 2) && (cblksty & J2K_CCP_CBLKSTY_LAZY)) ? T1_TYPE_RAW : T1_TYPE_MQ;
		/* FIXME: slviewer gets here with a null pointer. Why? Partially downloaded and/or corrupt textures? */
		if(seg->data == NULL){
			continue;
		}
		if (type == T1_TYPE_RAW) {
			raw_init_dec(raw, (*seg->data) + seg->dataindex, seg->len);
		} else {
			mqc_init_dec(mqc, (*seg->data) + seg->dataindex, seg->len);
		}
		
		for (passno = 0; passno < seg->numpasses; ++passno) {
			switch (passtype) {
				case 0:
					if (type == T1_TYPE_RAW) {
						t1_dec_sigpass_raw(t1, bpno+1, orient, cblksty);
					} else {
						if (cblksty & J2K_CCP_CBLKSTY_VSC) {
							t1_dec_sigpass_mqc_vsc(t1, bpno+1, orient);
						} else {
							t1_dec_sigpass_mqc(t1, bpno+1, orient);
						}
					}
					break;
				case 1:
					if (type == T1_TYPE_RAW) {
						t1_dec_refpass_raw(t1, bpno+1, cblksty);
					} else {
						if (cblksty & J2K_CCP_CBLKSTY_VSC) {
							t1_dec_refpass_mqc_vsc(t1, bpno+1);
						} else {
							t1_dec_refpass_mqc(t1, bpno+1);
						}
					}
					break;
				case 2:
					t1_dec_clnpass(t1, bpno+1, orient, cblksty);
					break;
			}
			
			if ((cblksty & J2K_CCP_CBLKSTY_RESET) && type == T1_TYPE_MQ) {
				mqc_resetstates(mqc);
				mqc_setstate(mqc, T1_CTXNO_UNI, 0, 46);				
				mqc_setstate(mqc, T1_CTXNO_AGG, 0, 3);
				mqc_setstate(mqc, T1_CTXNO_ZC, 0, 4);
			}
			if (++passtype == 3) {
				passtype = 0;
				bpno--;
			}
		}
	}
}

/* ----------------------------------------------------------------------- */

opj_t1_t* t1_create(opj_common_ptr cinfo) {
	opj_t1_t *t1 = (opj_t1_t*) opj_malloc(sizeof(opj_t1_t));
	if(!t1)
		return NULL;

	t1->cinfo = cinfo;
	/* create MQC and RAW handles */
	t1->mqc = mqc_create();
	t1->raw = raw_create();

	t1->data=NULL;
	t1->flags=NULL;
	t1->datasize=0;
	t1->flagssize=0;

	return t1;
}

void t1_destroy(opj_t1_t *t1) {
	if(t1) {
		/* destroy MQC and RAW handles */
		mqc_destroy(t1->mqc);
		raw_destroy(t1->raw);
		opj_aligned_free(t1->data);
		opj_aligned_free(t1->flags);
		opj_free(t1);
	}
}

void t1_encode_cblks(
		opj_t1_t *t1,
		opj_tcd_tile_t *tile,
		opj_tcp_t *tcp)
{
	S32 compno, resno, bandno, precno, cblkno;

	tile->distotile = 0;		/* fixed_quality */

	for (compno = 0; compno < tile->numcomps; ++compno) {
		opj_tcd_tilecomp_t* tilec = &tile->comps[compno];
		opj_tccp_t* tccp = &tcp->tccps[compno];
		S32 tile_w = tilec->x1 - tilec->x0;

		for (resno = 0; resno < tilec->numresolutions; ++resno) {
			opj_tcd_resolution_t *res = &tilec->resolutions[resno];

			for (bandno = 0; bandno < res->numbands; ++bandno) {
				opj_tcd_band_t* restrict band = &res->bands[bandno];

				for (precno = 0; precno < res->pw * res->ph; ++precno) {
					opj_tcd_precinct_t *prc = &band->precincts[precno];

					for (cblkno = 0; cblkno < prc->cw * prc->ch; ++cblkno) {
						opj_tcd_cblk_enc_t* cblk = &prc->cblks.enc[cblkno];
						S32* restrict datap;
						S32* restrict tiledp;
						S32 cblk_w;
						S32 cblk_h;
						S32 i, j;

						S32 x = cblk->x0 - band->x0;
						S32 y = cblk->y0 - band->y0;
						if (band->bandno & 1) {
							opj_tcd_resolution_t *pres = &tilec->resolutions[resno - 1];
							x += pres->x1 - pres->x0;
						}
						if (band->bandno & 2) {
							opj_tcd_resolution_t *pres = &tilec->resolutions[resno - 1];
							y += pres->y1 - pres->y0;
						}

						if(!allocate_buffers(
									t1,
									cblk->x1 - cblk->x0,
									cblk->y1 - cblk->y0))
						{
							return;
						}

						datap=t1->data;
						cblk_w = t1->w;
						cblk_h = t1->h;

						tiledp=&tilec->data[(y * tile_w) + x];
						if (tccp->qmfbid == 1) {
							for (j = 0; j < cblk_h; ++j) {
								for (i = 0; i < cblk_w; ++i) {
									S32 tmp = tiledp[(j * tile_w) + i];
									datap[(j * cblk_w) + i] = tmp << T1_NMSEDEC_FRACBITS;
								}
							}
						} else {		/* if (tccp->qmfbid == 0) */
							for (j = 0; j < cblk_h; ++j) {
								for (i = 0; i < cblk_w; ++i) {
									S32 tmp = tiledp[(j * tile_w) + i];
									datap[(j * cblk_w) + i] =
										fix_mul(
										tmp,
										8192 * 8192 / ((S32) floor(band->stepsize * 8192))) >> (11 - T1_NMSEDEC_FRACBITS);
								}
							}
						}

						t1_encode_cblk(
								t1,
								cblk,
								band->bandno,
								compno,
								tilec->numresolutions - 1 - resno,
								tccp->qmfbid,
								band->stepsize,
								tccp->cblksty,
								tile->numcomps,
								tcp->mct,
								tile);

					} /* cblkno */
				} /* precno */
			} /* bandno */
		} /* resno  */
	} /* compno  */
}

void t1_decode_cblks(
		opj_t1_t* t1,
		opj_tcd_tilecomp_t* tilec,
		opj_tccp_t* tccp)
{
	S32 resno, bandno, precno, cblkno;

	S32 tile_w = tilec->x1 - tilec->x0;

	for (resno = 0; resno < tilec->numresolutions; ++resno) {
		opj_tcd_resolution_t* res = &tilec->resolutions[resno];

		for (bandno = 0; bandno < res->numbands; ++bandno) {
			opj_tcd_band_t* restrict band = &res->bands[bandno];

			for (precno = 0; precno < res->pw * res->ph; ++precno) {
				opj_tcd_precinct_t* precinct = &band->precincts[precno];

				for (cblkno = 0; cblkno < precinct->cw * precinct->ch; ++cblkno) {
					opj_tcd_cblk_dec_t* cblk = &precinct->cblks.dec[cblkno];
					S32* restrict datap;
					S32 cblk_w, cblk_h;
					S32 x, y;
					S32 i, j;

					t1_decode_cblk(
							t1,
							cblk,
							band->bandno,
							tccp->roishift,
							tccp->cblksty);

					x = cblk->x0 - band->x0;
					y = cblk->y0 - band->y0;
					if (band->bandno & 1) {
						opj_tcd_resolution_t* pres = &tilec->resolutions[resno - 1];
						x += pres->x1 - pres->x0;
					}
					if (band->bandno & 2) {
						opj_tcd_resolution_t* pres = &tilec->resolutions[resno - 1];
						y += pres->y1 - pres->y0;
					}

					datap=t1->data;
					cblk_w = t1->w;
					cblk_h = t1->h;

					if (tccp->roishift) {
						S32 thresh = 1 << tccp->roishift;
						for (j = 0; j < cblk_h; ++j) {
							for (i = 0; i < cblk_w; ++i) {
								S32 val = datap[(j * cblk_w) + i];
								S32 mag = abs(val);
								if (mag >= thresh) {
									mag >>= tccp->roishift;
									datap[(j * cblk_w) + i] = val < 0 ? -mag : mag;
								}
							}
						}
					}

					if (tccp->qmfbid == 1) {
						S32* restrict tiledp = &tilec->data[(y * tile_w) + x];
						for (j = 0; j < cblk_h; ++j) {
							for (i = 0; i < cblk_w; ++i) {
								S32 tmp = datap[(j * cblk_w) + i];
								((S32*)tiledp)[(j * tile_w) + i] = tmp / 2;
							}
						}
					} else {		/* if (tccp->qmfbid == 0) */
						F32* restrict tiledp = (F32*) &tilec->data[(y * tile_w) + x];
						for (j = 0; j < cblk_h; ++j) {
							F32* restrict tiledp2 = tiledp;
							for (i = 0; i < cblk_w; ++i) {
								F32 tmp = *datap * band->stepsize;
								*tiledp2 = tmp;
								datap++;
								tiledp2++;
							}
							tiledp += tile_w;
						}
					}
					opj_free(cblk->data);
					opj_free(cblk->segs);
				} /* cblkno */
				opj_free(precinct->cblks.dec);
			} /* precno */
		} /* bandno */
	} /* resno */
}

