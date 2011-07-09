/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux and Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
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
#ifndef __J2K_H
#define __J2K_H
/**
@file j2k.h
@brief The JPEG-2000 Codestream Reader/Writer (J2K)

The functions in J2K.C have for goal to read/write the several parts of the codestream: markers and data.
*/

#include "openjpeg.h"
/** @defgroup J2K J2K - JPEG-2000 codestream reader/writer */
/*@{*/

#define J2K_CP_CSTY_PRT 0x01
#define J2K_CP_CSTY_SOP 0x02
#define J2K_CP_CSTY_EPH 0x04
#define J2K_CCP_CSTY_PRT 0x01
#define J2K_CCP_CBLKSTY_LAZY 0x01     /**< Selective arithmetic coding bypass */
#define J2K_CCP_CBLKSTY_RESET 0x02    /**< Reset context probabilities on coding pass boundaries */
#define J2K_CCP_CBLKSTY_TERMALL 0x04  /**< Termination on each coding pass */
#define J2K_CCP_CBLKSTY_VSC 0x08      /**< Vertically stripe causal context */
#define J2K_CCP_CBLKSTY_PTERM 0x10    /**< Predictable termination */
#define J2K_CCP_CBLKSTY_SEGSYM 0x20   /**< Segmentation symbols are used */
#define J2K_CCP_QNTSTY_NOQNT 0
#define J2K_CCP_QNTSTY_SIQNT 1
#define J2K_CCP_QNTSTY_SEQNT 2

/* ----------------------------------------------------------------------- */

#define J2K_MS_SOC 0xff4f	/**< SOC marker value */
#define J2K_MS_SOT 0xff90	/**< SOT marker value */
#define J2K_MS_SOD 0xff93	/**< SOD marker value */
#define J2K_MS_EOC 0xffd9	/**< EOC marker value */
#define J2K_MS_SIZ 0xff51	/**< SIZ marker value */
#define J2K_MS_COD 0xff52	/**< COD marker value */
#define J2K_MS_COC 0xff53	/**< COC marker value */
#define J2K_MS_RGN 0xff5e	/**< RGN marker value */
#define J2K_MS_QCD 0xff5c	/**< QCD marker value */
#define J2K_MS_QCC 0xff5d	/**< QCC marker value */
#define J2K_MS_POC 0xff5f	/**< POC marker value */
#define J2K_MS_TLM 0xff55	/**< TLM marker value */
#define J2K_MS_PLM 0xff57	/**< PLM marker value */
#define J2K_MS_PLT 0xff58	/**< PLT marker value */
#define J2K_MS_PPM 0xff60	/**< PPM marker value */
#define J2K_MS_PPT 0xff61	/**< PPT marker value */
#define J2K_MS_SOP 0xff91	/**< SOP marker value */
#define J2K_MS_EPH 0xff92	/**< EPH marker value */
#define J2K_MS_CRG 0xff63	/**< CRG marker value */
#define J2K_MS_COM 0xff64	/**< COM marker value */
/* UniPG>> */
#ifdef USE_JPWL
#define J2K_MS_EPC 0xff68	/**< EPC marker value (Part 11: JPEG 2000 for Wireless) */
#define J2K_MS_EPB 0xff66	/**< EPB marker value (Part 11: JPEG 2000 for Wireless) */ 
#define J2K_MS_ESD 0xff67	/**< ESD marker value (Part 11: JPEG 2000 for Wireless) */ 
#define J2K_MS_RED 0xff69	/**< RED marker value (Part 11: JPEG 2000 for Wireless) */
#endif /* USE_JPWL */
#ifdef USE_JPSEC
#define J2K_MS_SEC 0xff65    /**< SEC marker value (Part 8: Secure JPEG 2000) */
#define J2K_MS_INSEC 0xff94  /**< INSEC marker value (Part 8: Secure JPEG 2000) */
#endif /* USE_JPSEC */
/* <<UniPG */


/* ----------------------------------------------------------------------- */

/**
Values that specify the status of the decoding process when decoding the main header. 
These values may be combined with a | operator. 
*/
typedef enum J2K_STATUS {
	J2K_STATE_MHSOC  = 0x0001, /**< a SOC marker is expected */
	J2K_STATE_MHSIZ  = 0x0002, /**< a SIZ marker is expected */
	J2K_STATE_MH     = 0x0004, /**< the decoding process is in the main header */
	J2K_STATE_TPHSOT = 0x0008, /**< the decoding process is in a tile part header and expects a SOT marker */
	J2K_STATE_TPH    = 0x0010, /**< the decoding process is in a tile part header */
	J2K_STATE_MT     = 0x0020, /**< the EOC marker has just been read */
	J2K_STATE_NEOC   = 0x0040, /**< the decoding process must not expect a EOC marker because the codestream is truncated */
	J2K_STATE_ERR    = 0x0080  /**< the decoding process has encountered an error */
} J2K_STATUS;

/* ----------------------------------------------------------------------- */

/** 
T2 encoding mode 
*/
typedef enum T2_MODE {
	THRESH_CALC = 0,	/** Function called in Rate allocation process*/
	FINAL_PASS = 1		/** Function called in Tier 2 process*/
}J2K_T2_MODE;

/**
Quantization stepsize
*/
typedef struct opj_stepsize {
	/** exponent */
	S32 expn;
	/** mantissa */
	S32 mant;
} opj_stepsize_t;

/**
Tile-component coding parameters
*/
typedef struct opj_tccp {
	/** coding style */
	S32 csty;
	/** number of resolutions */
	S32 numresolutions;
	/** code-blocks width */
	S32 cblkw;
	/** code-blocks height */
	S32 cblkh;
	/** code-block coding style */
	S32 cblksty;
	/** discrete wavelet transform identifier */
	S32 qmfbid;
	/** quantisation style */
	S32 qntsty;
	/** stepsizes used for quantization */
	opj_stepsize_t stepsizes[J2K_MAXBANDS];
	/** number of guard bits */
	S32 numgbits;
	/** Region Of Interest shift */
	S32 roishift;
	/** precinct width */
	S32 prcw[J2K_MAXRLVLS];
	/** precinct height */
	S32 prch[J2K_MAXRLVLS];	
} opj_tccp_t;

/**
Tile coding parameters : 
this structure is used to store coding/decoding parameters common to all
tiles (information like COD, COC in main header)
*/
typedef struct opj_tcp {
	/** 1 : first part-tile of a tile */
	S32 first;
	/** coding style */
	S32 csty;
	/** progression order */
	OPJ_PROG_ORDER prg;
	/** number of layers */
	S32 numlayers;
	/** multi-component transform identifier */
	S32 mct;
	/** rates of layers */
	F32 rates[100];
	/** number of progression order changes */
	S32 numpocs;
	/** indicates if a POC marker has been used O:NO, 1:YES */
	S32 POC;
	/** progression order changes */
	opj_poc_t pocs[32];
	/** packet header store there for futur use in t2_decode_packet */
	unsigned char *ppt_data;
	/** pointer remaining on the first byte of the first header if ppt is used */
	unsigned char *ppt_data_first;
	/** If ppt == 1 --> there was a PPT marker for the present tile */
	S32 ppt;
	/** used in case of multiple marker PPT (number of info already stored) */
	S32 ppt_store;
	/** ppmbug1 */
	S32 ppt_len;
	/** add fixed_quality */
	F32 distoratio[100];
	/** tile-component coding parameters */
	opj_tccp_t *tccps;
} opj_tcp_t;

/**
Coding parameters
*/
typedef struct opj_cp {
	/** Digital cinema profile*/
	OPJ_CINEMA_MODE cinema;
	/** Maximum rate for each component. If == 0, component size limitation is not considered */
	S32 max_comp_size;
	/** Size of the image in bits*/
	S32 img_size;
	/** Rsiz*/
	OPJ_RSIZ_CAPABILITIES rsiz;
	/** Enabling Tile part generation*/
	char tp_on;
	/** Flag determining tile part generation*/
	char tp_flag;
	/** Position of tile part flag in progression order*/
	S32 tp_pos;
	/** allocation by rate/distortion */
	S32 disto_alloc;
	/** allocation by fixed layer */
	S32 fixed_alloc;
	/** add fixed_quality */
	S32 fixed_quality;
	/** if != 0, then original dimension divided by 2^(reduce); if == 0 or not used, image is decoded to the full resolution */
	S32 reduce;
	/** if != 0, then only the first "layer" layers are decoded; if == 0 or not used, all the quality layers are decoded */
	S32 layer;
	/** if == NO_LIMITATION, decode entire codestream; if == LIMIT_TO_MAIN_HEADER then only decode the main header */
	OPJ_LIMIT_DECODING limit_decoding;
	/** XTOsiz */
	S32 tx0;
	/** YTOsiz */
	S32 ty0;
	/** XTsiz */
	S32 tdx;
	/** YTsiz */
	S32 tdy;
	/** comment for coding */
	char *comment;
	/** number of tiles in width */
	S32 tw;
	/** number of tiles in heigth */
	S32 th;
	/** ID number of the tiles present in the codestream */
	S32 *tileno;
	/** size of the vector tileno */
	S32 tileno_size;
	/** packet header store there for futur use in t2_decode_packet */
	unsigned char *ppm_data;
	/** pointer remaining on the first byte of the first header if ppm is used */
	unsigned char *ppm_data_first;
	/** if ppm == 1 --> there was a PPM marker for the present tile */
	S32 ppm;
	/** use in case of multiple marker PPM (number of info already store) */
	S32 ppm_store;
	/** use in case of multiple marker PPM (case on non-finished previous info) */
	S32 ppm_previous;
	/** ppmbug1 */
	S32 ppm_len;
	/** tile coding parameters */
	opj_tcp_t *tcps;
	/** fixed layer */
	S32 *matrice;
/* UniPG>> */
#ifdef USE_JPWL
	/** enables writing of EPC in MH, thus activating JPWL */
	bool epc_on;
	/** enables writing of EPB, in case of activated JPWL */
	bool epb_on;
	/** enables writing of ESD, in case of activated JPWL */
	bool esd_on;
	/** enables writing of informative techniques of ESD, in case of activated JPWL */
	bool info_on;
	/** enables writing of RED, in case of activated JPWL */
	bool red_on;
	/** error protection method for MH (0,1,16,32,37-128) */
	S32 hprot_MH;
	/** tile number of header protection specification (>=0) */
	S32 hprot_TPH_tileno[JPWL_MAX_NO_TILESPECS];
	/** error protection methods for TPHs (0,1,16,32,37-128) */
	S32 hprot_TPH[JPWL_MAX_NO_TILESPECS];
	/** tile number of packet protection specification (>=0) */
	S32 pprot_tileno[JPWL_MAX_NO_PACKSPECS];
	/** packet number of packet protection specification (>=0) */
	S32 pprot_packno[JPWL_MAX_NO_PACKSPECS];
	/** error protection methods for packets (0,1,16,32,37-128) */
	S32 pprot[JPWL_MAX_NO_PACKSPECS];
	/** enables writing of ESD, (0/2/4 bytes) */
	S32 sens_size;
	/** sensitivity addressing size (0=auto/2/4 bytes) */
	S32 sens_addr;
	/** sensitivity range (0-3) */
	S32 sens_range;
	/** sensitivity method for MH (-1,0-7) */
	S32 sens_MH;
	/** tile number of sensitivity specification (>=0) */
	S32 sens_TPH_tileno[JPWL_MAX_NO_TILESPECS];
	/** sensitivity methods for TPHs (-1,0-7) */
	S32 sens_TPH[JPWL_MAX_NO_TILESPECS];
	/** enables JPWL correction at the decoder */
	bool correct;
	/** expected number of components at the decoder */
	S32 exp_comps;
	/** maximum number of tiles at the decoder */
	S32 max_tiles;
#endif /* USE_JPWL */
/* <<UniPG */
} opj_cp_t;

/**
JPEG-2000 codestream reader/writer
*/
typedef struct opj_j2k {
	/** codec context */
	opj_common_ptr cinfo;

	/** locate in which part of the codestream the decoder is (main header, tile header, end) */
	S32 state;
	/** number of the tile curently concern by coding/decoding */
	S32 curtileno;
	/** Tile part number*/
	S32 tp_num;
	/** Tilepart number currently coding*/
	S32 cur_tp_num;
	/** Total number of tileparts of the current tile*/
	S32 *cur_totnum_tp;
	/**
	locate the start position of the TLM marker  
	after encoding the tilepart, a jump (in j2k_write_sod) is done to the TLM marker to store the value of its length. 
	*/
	S32 tlm_start;
	/** Total num of tile parts in whole image = num tiles* num tileparts in each tile*/
	/** used in TLMmarker*/
	S32 totnum_tp;	
	/** 
	locate the position of the end of the tile in the codestream, 
	used to detect a truncated codestream (in j2k_read_sod)
	*/
	unsigned char *eot;
	/**
	locate the start position of the SOT marker of the current coded tile:  
	after encoding the tile, a jump (in j2k_write_sod) is done to the SOT marker to store the value of its length. 
	*/
	S32 sot_start;
	S32 sod_start;
	/**
	as the J2K-file is written in several parts during encoding, 
	it enables to make the right correction in position return by cio_tell
	*/
	S32 pos_correction;
	/** array used to store the data of each tile */
	unsigned char **tile_data;
	/** array used to store the length of each tile */
	S32 *tile_len;
	/** 
	decompression only : 
	store decoding parameters common to all tiles (information like COD, COC in main header)
	*/
	opj_tcp_t *default_tcp;
	/** pointer to the encoded / decoded image */
	opj_image_t *image;
	/** pointer to the coding parameters */
	opj_cp_t *cp;
	/** helper used to write the index file */
	opj_codestream_info_t *cstr_info;
	/** pointer to the byte i/o stream */
	opj_cio_t *cio;
} opj_j2k_t;

/** @name Exported functions */
/*@{*/
/* ----------------------------------------------------------------------- */
/**
Creates a J2K decompression structure
@param cinfo Codec context info
@return Returns a handle to a J2K decompressor if successful, returns NULL otherwise
*/
opj_j2k_t* j2k_create_decompress(opj_common_ptr cinfo);
/**
Destroy a J2K decompressor handle
@param j2k J2K decompressor handle to destroy
*/
void j2k_destroy_decompress(opj_j2k_t *j2k);
/**
Setup the decoder decoding parameters using user parameters.
Decoding parameters are returned in j2k->cp. 
@param j2k J2K decompressor handle
@param parameters decompression parameters
*/
void j2k_setup_decoder(opj_j2k_t *j2k, opj_dparameters_t *parameters);
/**
Decode an image from a JPEG-2000 codestream
@param j2k J2K decompressor handle
@param cio Input buffer stream
@param cstr_info Codestream information structure if required, NULL otherwise
@return Returns a decoded image if successful, returns NULL otherwise
*/
opj_image_t* j2k_decode(opj_j2k_t *j2k, opj_cio_t *cio, opj_codestream_info_t *cstr_info);
/**
Decode an image form a JPT-stream (JPEG 2000, JPIP)
@param j2k J2K decompressor handle
@param cio Input buffer stream
@param cstr_info Codestream information structure if required, NULL otherwise
@return Returns a decoded image if successful, returns NULL otherwise
*/
opj_image_t* j2k_decode_jpt_stream(opj_j2k_t *j2k, opj_cio_t *cio, opj_codestream_info_t *cstr_info);
/**
Creates a J2K compression structure
@param cinfo Codec context info
@return Returns a handle to a J2K compressor if successful, returns NULL otherwise
*/
opj_j2k_t* j2k_create_compress(opj_common_ptr cinfo);
/**
Destroy a J2K compressor handle
@param j2k J2K compressor handle to destroy
*/
void j2k_destroy_compress(opj_j2k_t *j2k);
/**
Setup the encoder parameters using the current image and using user parameters. 
Coding parameters are returned in j2k->cp. 
@param j2k J2K compressor handle
@param parameters compression parameters
@param image input filled image
*/
void j2k_setup_encoder(opj_j2k_t *j2k, opj_cparameters_t *parameters, opj_image_t *image);
/**
Converts an enum type progression order to string type
*/
char *j2k_convert_progression_order(OPJ_PROG_ORDER prg_order);
/**
Encode an image into a JPEG-2000 codestream
@param j2k J2K compressor handle
@param cio Output buffer stream
@param image Image to encode
@param cstr_info Codestream information structure if required, NULL otherwise
@return Returns true if successful, returns false otherwise
*/
bool j2k_encode(opj_j2k_t *j2k, opj_cio_t *cio, opj_image_t *image, opj_codestream_info_t *cstr_info);

/* ----------------------------------------------------------------------- */
/*@}*/

/*@}*/

#endif /* __J2K_H */
