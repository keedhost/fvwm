/* -*-c-*- */
/* Copyright (C) 2002  Olivier Chapuis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* FlocaleRotateDrawString is strongly inspired by some part of xvertext
 * taken from wmx */
/* Here the copyright for this function: */
/* xvertext, Copyright (c) 1992 Alan Richardson (mppa3@uk.ac.sussex.syma)
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both the
 * copyright notice and this permission notice appear in supporting
 * documentation.  All work developed as a consequence of the use of
 * this program should duly acknowledge such use. No representations are
 * made about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * Minor modifications by Chris Cannam for wm2/wmx
 * Major modifications by Kazushi (Jam) Marukawa for wm2/wmx i18n patch
 * Simplification and complications by olicha for use with fvwm
 */

/* ---------------------------- included header files ----------------------- */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "defaults.h"
#include "fvwmlib.h"
#include "safemalloc.h"
#include "Strings.h"
#include "Parse.h"
#include "PictureBase.h"
#include "Flocale.h"
#include "FlocaleCharset.h"
#include "FBidi.h"
#include "FftInterface.h"
#include "Colorset.h"
#include "Ficonv.h"

/* ---------------------------- local definitions --------------------------- */

/* ---------------------------- local macros -------------------------------- */

#define FSwitchDrawString(use_16, dpy, d, gc, x, y, s8, s16, l) \
	(use_16) ? \
	XDrawString16(dpy, d, gc, x, y, s16, l) : \
	XDrawString(dpy, d, gc, x, y, s8, l)
#define FSwitchDrawImageString(use_16, dpy, d, gc, x, y, s8, s16, l) \
	(use_16) ? \
	XDrawImageString16(dpy, d, gc, x, y, s16, l) : \
	XDrawImageString(dpy, d, gc, x, y, s8, l)

/* ---------------------------- imports ------------------------------------- */

/* ---------------------------- included code files ------------------------- */

/* ---------------------------- local types --------------------------------- */

/* ---------------------------- forward declarations ------------------------ */

/* ---------------------------- local variables ----------------------------- */

static FlocaleFont *FlocaleFontList = NULL;
static char *Flocale = NULL;
static char *Fmodifiers = NULL;

/* ---------------------------- exported variables (globals) ---------------- */

/* ---------------------------- local functions ----------------------------- */

/* ***************************************************************************
 * shadow local functions
 * ***************************************************************************/

static
void FlocaleParseShadow(char *str, int *shadow_size, int *shadow_offset,
			int *direction, char * fontname, char *module)
{
	char *dir_str;
	char *token;
	multi_direction_type dir;

	*direction = MULTI_DIR_NONE;
	token = PeekToken(str, &dir_str);
	if (token == NULL || *token == 0 ||
	    (GetIntegerArguments(token, NULL, shadow_size, 1) != 1) ||
	    *shadow_size < 0)
	{
		*shadow_size = 0;
		fprintf(stderr,"[%s][FlocaleParseShadow]: WARNING -- bad "
			"shadow size in font name:\n\t'%s'\n",
			(module)? module: "FVWM", fontname);
		return;
	}
	if (*shadow_size == 0)
	{
		return;
	}
	/* some offset ? */
	if (dir_str && *dir_str &&
	    (GetIntegerArguments(dir_str, NULL, shadow_offset, 1) == 1))
	{
		if (*shadow_offset < 0)
		{
			*shadow_offset = 0;
			fprintf(stderr,"[%s][FlocaleParseShadow]: WARNING -- "
				"bad shadow offset in font name:\n\t'%s'\n",
				(module)? module: "FVWMlibs", fontname);
		}
		PeekToken(dir_str, &dir_str);
	}
	while (dir_str && *dir_str && *dir_str != '\n')
	{
		dir = gravity_parse_multi_dir_argument(dir_str, &dir_str);
		if (dir == MULTI_DIR_NONE)
		{
			fprintf(stderr,"[%s][FlocaleParseShadow]: WARNING -- "
				"bad shadow direction in font description:\n"
				"\t%s\n",
				(module)? module: "FVWMlibs", fontname);
			PeekToken(dir_str, &dir_str); /* skip it */
		}
		else
		{
			*direction |= dir;
		}
	}
	if (*direction == MULTI_DIR_NONE)
		*direction = MULTI_DIR_SE;
}

/* ***************************************************************************
 * some simple converters
 * ***************************************************************************/

static
XChar2b *FlocaleUtf8ToUnicodeStr2b(unsigned char *str, int len, int *nl)
{
	XChar2b *str2b = NULL;
	int i = 0, j = 0, t;

	str2b = (XChar2b *)safemalloc((len+1)*sizeof(XChar2b));
	while (i < len && str[i] != 0)
	{
		if (str[i] <= 0x7f)
		{
			str2b[j].byte2 = str[i];
			str2b[j].byte1 = 0;
		}
		else if (str[i] <= 0xdf && i+1 < len)
		{
		    t = ((str[i] & 0x1f) << 6) + (str[i+1] & 0x3f);
		    str2b[j].byte2 = (unsigned char)(t & 0xff);
		    str2b[j].byte1 = (unsigned char)(t >> 8);
		    i++;
		}
		else if (i+2 <len)
		{
			t = ((str[i] & 0x0f) << 12) + ((str[i+1] & 0x3f) << 6)+
				(str[i+2] & 0x3f);
			str2b[j].byte2 = (unsigned char)(t & 0xff);
			str2b[j].byte1 = (unsigned char)(t >> 8);
			i += 2;
		}
		i++; j++;
	}
	*nl = j;
	return str2b;
}

/* Note: this function is not expected to work; good mb rendering
 * should be (and is) done using Xmb functions and not XDrawString16
 * (or with iso10646-1 fonts and setting the encoding).
 * This function is used when the locale does not correspond to the font.
 * It works with  "EUC fonts": ksc5601.1987-0, gb2312 and maybe also cns11643-*.
 * It works patially with jisx* and big5-0. Should try gbk-0, big5hkscs-0,
 * and cns-11643- */
static
XChar2b *FlocaleStringToString2b(
	Display *dpy, FlocaleFont *flf, unsigned char *str, int len, int *nl)
{
	XChar2b *str2b = NULL;
	char *tmp = NULL;
	Bool free_str = False;
	int i = 0, j = 0;
	Bool euc = True; /* KSC5601 (EUC-KR), GB2312 (EUC-CN), CNS11643-1986-1
			  * (EUC-TW) and converted  jisx (EUC-JP) */

	if (flf->fc && StrEquals(flf->fc->x,"jisx0208.1983-0"))
	{
		tmp = FiconvCharsetToCharset(
			dpy, flf->fc, FlocaleCharsetGetEUCJPCharset(), str, len);
		if (tmp != NULL)
		{
			free_str = True;
			str = tmp;
			len = strlen(tmp);
		}
	}
	else if (flf->fc && StrEquals(flf->fc->x,"big5-0"))
	{
		euc = False;
	}
	str2b = (XChar2b *)safemalloc((len+1)*sizeof(XChar2b));
	if (euc)
	{
		while (i < len && str[i] != 0)
		{
			if (str[i] <= 0x7f)
			{
				/* seems ok with KSC5601 and GB2312 as we get
				 * almost the ascii. I do no try CNS11643-1986-1.
				 * Should convert to ascii with jisx */
				str2b[j].byte1 = 0x23; /* magic number! */
				str2b[j].byte2 = str[i++];
			}
			else if (i+1 < len)
			{
				/* mb gl (for gr replace & 0x7f by | 0x80 ...)*/
				str2b[j].byte1 = str[i++] & 0x7f;
				str2b[j].byte2 = str[i++] & 0x7f;
			}
			else
			{
				str2b[j].byte1 = 0;
				str2b[j].byte2 = 0;
				i++;
			}
			j++;
		}
	}
	else /* big5 and others not yet tested */
	{
		while (i < len && str[i] != 0)
		{
			if (str[i] <= 0x7f)
			{
				/* we should convert to ascii */
#if 0
				str2b[j].byte1 = 0xa2; /* magic number! */
				str2b[j].byte2 = str[i++];
#endif
				/* a blanck char ... */
				str2b[j].byte1 = 0x21;
				str2b[j].byte2 = 0x21;
			}
			else if (i+1 < len)
			{
				str2b[j].byte1 = str[i++];
				str2b[j].byte2 = str[i++];
			}
			else
			{
				str2b[j].byte1 = 0;
				str2b[j].byte2 = 0;
				i++;
			}
			j++;
		}
	}
	*nl = j;
	if (free_str)
		free(str);
	return str2b;
}

static
char *FlocaleEncodeString(
	Display *dpy, FlocaleFont *flf, char *str, int *do_free, int len,
	int *nl, int *is_rtl)
{
	char *str1, *str2;
	int len1 = len, len2;
	Bool do_iconv = True;
	const char *bidi_charset;

	if (is_rtl != NULL)
		*is_rtl = False;
	*do_free = False;
	*nl = len;

	if (flf->str_fc == NULL || flf->fc == NULL ||
	    flf->fc == flf->str_fc)
	{
		do_iconv = False;
	}

	str1 = str;
	if (FiconvSupport && do_iconv)
	{
		str1 = FiconvCharsetToCharset(
			dpy, flf->str_fc, flf->fc, (const char *)str, len);
		if (str1 == NULL)
		{
			/* fail to convert */
			*do_free = False;
			return str;
		}
		if (str1 != str)
		{
			*do_free = True;
			len1 = strlen(str1);
		}
	}

	if (FlocaleGetBidiCharset(dpy, flf->str_fc) != NULL &&
	    (bidi_charset = FlocaleGetBidiCharset(dpy, flf->fc)) != NULL)
	{
		str2 = FBidiConvert(str1, bidi_charset, len1, is_rtl, &len2);
		if (str2 != NULL && str2  != str1)
		{
			if (*do_free)
			{
				free(str1);
			}
			*do_free = True;
			len1 = len2;
			str1 = str2;
		}
	}
	*nl = len1;
	return str1;
}

static
void FlocaleEncodeWinString(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws, int *do_free,
	int *len)
{
	fws->e_str = FlocaleEncodeString(
		dpy, flf, fws->str, do_free, *len, len, NULL);
	fws->str2b = NULL;

	if (flf->font != None)
	{
		if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
		{
			fws->str2b = FlocaleUtf8ToUnicodeStr2b(
							fws->e_str, *len, len);
		}
		else if (flf->flags.is_mb)
		{
			fws->str2b = FlocaleStringToString2b(
					dpy, flf, fws->e_str, *len, len);
		}
		if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc) || flf->flags.is_mb)
		{
			if (*do_free)
			{
				free(fws->e_str);
			}
			fws->e_str = NULL;
			*do_free = True;
		}
	}
}

/* ***************************************************************************
 * Text Drawing with a FontStruct
 * ***************************************************************************/

static
void FlocaleFontStructDrawString(Display *dpy, FlocaleFont *flf, Drawable d,
				 GC gc, int x, int y, Pixel fg, Pixel fgsh,
				 Bool has_fg_pixels, FlocaleWinString *fws,
				 int len, Bool image)
{
	int xt = x;
	int yt = y;
	int is_string16;
	flocale_gstp_args gstp_args;

	is_string16 = (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc) || flf->flags.is_mb);
	if (is_string16 && fws->str2b == NULL)
	{
		return;
	}
	if (image)
	{
		/* for rotated drawing */
		FSwitchDrawImageString(
			is_string16, dpy, d, gc, x, y, fws->e_str, fws->str2b,
			len);
	}
	else
	{
		FlocaleInitGstpArgs(&gstp_args, flf, fws, x, y);
		/* normal drawing */
		if (flf->shadow_size != 0 && has_fg_pixels)
		{
			XSetForeground(dpy, fws->gc, fgsh);
			while (FlocaleGetShadowTextPosition(
				       &xt, &yt, &gstp_args))
			{
				FSwitchDrawString(
					is_string16, dpy, d, gc, xt, yt,
					fws->e_str, fws->str2b, len);
			}
			XSetForeground(dpy, gc, fg);
		}
		xt = gstp_args.orig_x;
		yt = gstp_args.orig_y;
		FSwitchDrawString(
			is_string16, dpy, d, gc, xt,yt, fws->e_str, fws->str2b,
			len);
	}

	return;
}

/* ***************************************************************************
 * Rotated Text Drawing with a FontStruct or a FontSet
 * ***************************************************************************/
static
void FlocaleRotateDrawString(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws, Pixel fg,
	Pixel fgsh, Bool has_fg_pixels, int len)
{
	static GC my_gc = None;
	static GC font_gc = None;
	int j, i, xpfg, ypfg, xpsh, ypsh;
	unsigned char *normal_data, *rotated_data;
	unsigned int normal_w, normal_h, normal_len;
	unsigned int rotated_w, rotated_h, rotated_len;
	char val;
	int width, height, descent;
	XImage *image, *rotated_image;
	Pixmap canvas_pix, rotated_pix;
	flocale_gstp_args gstp_args;

	if (fws->str == NULL || len < 1)
		return;
	if (fws->flags.text_rotation == ROTATION_0)
		return; /* should not happen */

	if (my_gc == None)
	{
		my_gc = fvwmlib_XCreateGC(dpy, fws->win, 0, NULL);
	}
	XCopyGC(dpy, fws->gc, GCForeground|GCBackground, my_gc);

	/* width and height (no shadow!) */
	width = FlocaleTextWidth(flf, fws->str, len) - FLF_SHADOW_WIDTH(flf);
	height = flf->height - FLF_SHADOW_HEIGHT(flf);
	descent = flf->descent - FLF_SHADOW_DESCENT(flf);;

	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	/* glyph width and height of the normal text */
	normal_w = width;
	normal_h = height;

	/* width in bytes */
	normal_len = (normal_w - 1) / 8 + 1;

	/* create and clear the canvas */
	canvas_pix = XCreatePixmap(dpy, fws->win, width, height, 1);
	if (font_gc == None)
	{
		font_gc = fvwmlib_XCreateGC(dpy, canvas_pix, 0, NULL);
	}
	XSetBackground(dpy, font_gc, 0);
	XSetForeground(dpy, font_gc, 0);
	XFillRectangle(dpy, canvas_pix, font_gc, 0, 0, width, height);

	/* draw the character center top right on canvas */
	XSetForeground(dpy, font_gc, 1);
	if (flf->font != NULL)
	{
		XSetFont(dpy, font_gc, flf->font->fid);
		FlocaleFontStructDrawString(dpy, flf, canvas_pix, font_gc, 0,
					    height - descent,
					    fg, fgsh, has_fg_pixels,
					    fws, len, True);
	}
	else if (flf->fontset != None)
	{
		XmbDrawString(
			dpy, canvas_pix, flf->fontset, font_gc, 0,
			height - descent, fws->e_str, len);
	}

	/* reserve memory for the first XImage */
	normal_data = (unsigned char *)safemalloc(normal_len * normal_h);

	/* create depth 1 XImage */
	if ((image = XCreateImage(dpy, Pvisual, 1, XYBitmap,
		0, (char *)normal_data, normal_w, normal_h, 8, 0)) == NULL)
	{
		return;
	}
	image->byte_order = image->bitmap_bit_order = MSBFirst;

	/* extract character from canvas */
	XGetSubImage(
		dpy, canvas_pix, 0, 0, normal_w, normal_h,
		1, XYPixmap, image, 0, 0);
	image->format = XYBitmap;

	/* width, height of the rotated text */
	if (fws->flags.text_rotation == ROTATION_180)
	{
		rotated_w = normal_w;
		rotated_h = normal_h;
	}
	else /* vertical text */
	{
		rotated_w = normal_h;
		rotated_h = normal_w;
	}

	/* width in bytes */
	rotated_len = (rotated_w - 1) / 8 + 1;

	/* reserve memory for the rotated image */
	rotated_data = (unsigned char *)safecalloc(rotated_h * rotated_len, 1);

	/* create the rotated X image */
	if ((rotated_image = XCreateImage(
		dpy, Pvisual, 1, XYBitmap, 0, (char *)rotated_data,
		rotated_w, rotated_h, 8, 0)) == NULL)
	{
		return;
	}

	rotated_image->byte_order = rotated_image->bitmap_bit_order = MSBFirst;

	/* map normal text data to rotated text data */
	for (j = 0; j < rotated_h; j++)
	{
		for (i = 0; i < rotated_w; i++)
		{
			/* map bits ... */
			if (fws->flags.text_rotation == ROTATION_270)
				val = normal_data[
					i * normal_len +
					(normal_w - j - 1) / 8
				] & (128 >> ((normal_w - j - 1) % 8));

			else if (fws->flags.text_rotation == ROTATION_180)
				val = normal_data[
					(normal_h - j - 1) * normal_len +
					(normal_w - i - 1) / 8
				] & (128 >> ((normal_w - i - 1) % 8));

			else /* ROTATION_90 */
				val = normal_data[
					(normal_h - i - 1) * normal_len +
					j / 8
				] & (128 >> (j % 8));

			if (val)
				rotated_data[j * rotated_len + i / 8] |=
					(128 >> (i % 8));
		}
	}

	/* create the character's bitmap  and put the image on it */
	rotated_pix = XCreatePixmap(dpy, fws->win, rotated_w, rotated_h, 1);
	XPutImage(
		dpy, rotated_pix, font_gc, rotated_image, 0, 0, 0, 0,
		rotated_w, rotated_h);

	/* free the image and data  */
	XDestroyImage(image);
	XDestroyImage(rotated_image);

	/* free pixmap and GC */
	XFreePixmap(dpy, canvas_pix);
	XFreeGC(dpy, font_gc);

	/* x and y corrections: we fill a rectangle! */
	switch (fws->flags.text_rotation)
	{
	case ROTATION_90:
		/* CW */
		xpfg = fws->x - (flf->height - flf->ascent);
		ypfg = fws->y;
		break;
	case ROTATION_180:
		xpfg = fws->x;
		ypfg = fws->y - (flf->height - flf->ascent) +
			FLF_SHADOW_BOTTOM_SIZE(flf);
		break;
	case ROTATION_270:
		/* CCW */
		xpfg = fws->x - flf->ascent;
		ypfg = fws->y;
		break;
	case ROTATION_0:
	default:
		xpfg = fws->x;
		ypfg = fws->y;
		break;
	}
	xpsh = xpfg;
	ypsh = ypfg;
	/* write the image on the window */
	XSetFillStyle(dpy, my_gc, FillStippled);
	XSetStipple(dpy, my_gc, rotated_pix);
	FlocaleInitGstpArgs(&gstp_args, flf, fws, xpfg, ypfg);
	if (flf->shadow_size != 0 && has_fg_pixels)
	{
		XSetForeground(dpy, my_gc, fgsh);
		while (FlocaleGetShadowTextPosition(&xpsh, &ypsh, &gstp_args))
		{
			XSetTSOrigin(dpy, my_gc, xpsh, ypsh);
			XFillRectangle(
				dpy, fws->win, my_gc, xpsh, ypsh, rotated_w,
				rotated_h);
		}
		XSetForeground(dpy, my_gc, fg);
	}
	xpsh = gstp_args.orig_x;
	ypsh = gstp_args.orig_y;
	XSetTSOrigin(dpy, my_gc, xpsh, ypsh);
	XFillRectangle(dpy, fws->win, my_gc, xpsh, ypsh, rotated_w, rotated_h);
	XFreePixmap(dpy, rotated_pix);
	XFreeGC(dpy, my_gc);

	return;
}

/* ***************************************************************************
 * Fonts loading
 * ***************************************************************************/

static
FlocaleFont *FlocaleGetFftFont(
	Display *dpy, char *fontname, char *encoding, char *module)
{
	FftFontType *fftf = NULL;
	FlocaleFont *flf = NULL;
	char *fn, *hints = NULL;

	hints = GetQuotedString(fontname, &fn, "/", NULL, NULL, NULL);
	fftf = FftGetFont(dpy, fn, module);
	if (fftf == NULL)
	{
		if (fn != NULL)
			free(fn);
		return NULL;
	}
	flf = (FlocaleFont *)safemalloc(sizeof(FlocaleFont));
	memset(flf, '\0', sizeof(FlocaleFont));
	flf->count = 1;
	flf->fftf = *fftf;
	FlocaleCharsetSetFlocaleCharset(dpy, flf, hints, encoding, module);
	FftGetFontHeights(
		&flf->fftf, &flf->height, &flf->ascent, &flf->descent);
	FftGetFontWidths(
		flf, &flf->max_char_width);
	free(fftf);
	if (fn != NULL)
		free(fn);
	return flf;
}

static
FlocaleFont *FlocaleGetFontSet(
	Display *dpy, char *fontname, char *encoding, char *module)
{
	static int mc_errors = 0;
	FlocaleFont *flf = NULL;
	XFontSet fontset = NULL;
	char **ml;
	int mc,i;
	char *ds;
	XFontSetExtents *fset_extents;
	char *fn, *hints = NULL;

	hints = GetQuotedString(fontname, &fn, "/", NULL, NULL, NULL);
	if (!(fontset = XCreateFontSet(dpy, fn, &ml, &mc, &ds)))
	{
		if (fn != NULL)
			free(fn);
		return NULL;
	}

	if (mc > 0)
	{
		if (mc_errors <= FLOCALE_NUMBER_MISS_CSET_ERR_MSG)
		{
			mc_errors++;
			fprintf(stderr,
				"[%s][FlocaleGetFontSet]: (%s)"
				" Missing font charsets:\n",
				(module)? module: "FVWMlibs", fontname);
			for (i = 0; i < mc; i++)
			{
				fprintf(stderr, "%s", ml[i]);
				if (i < mc - 1)
					fprintf(stderr, ", ");
			}
			fprintf(stderr, "\n");
			if (mc_errors == FLOCALE_NUMBER_MISS_CSET_ERR_MSG)
			{
				fprintf(stderr,
					"[%s][FlocaleGetFontSet]:"
					" No more missing charset reportings\n",
					(module)? module: "FVWMlibs");
			}
		}
		XFreeStringList(ml);
	}

	flf = (FlocaleFont *)safemalloc(sizeof(FlocaleFont));
	memset(flf, '\0', sizeof(FlocaleFont));
	flf->count = 1;
	flf->fontset = fontset;
	FlocaleCharsetSetFlocaleCharset(dpy, flf, hints, encoding, module);
	fset_extents = XExtentsOfFontSet(fontset);
	flf->height = fset_extents->max_logical_extent.height;
	flf->ascent = - fset_extents->max_logical_extent.y;
	flf->descent = fset_extents->max_logical_extent.height +
		fset_extents->max_logical_extent.y;
	flf->max_char_width = fset_extents->max_logical_extent.width;
	if (fn != NULL)
		free(fn);

	return flf;
}

static
FlocaleFont *FlocaleGetFont(
	Display *dpy, char *fontname, char *encoding, char *module)
{
	XFontStruct *font = NULL;
	FlocaleFont *flf;
	char *str,*fn,*tmp;
	char *hints = NULL;

	hints = GetQuotedString(fontname, &tmp, "/", NULL, NULL, NULL);
	str = GetQuotedString(tmp, &fn, ",", NULL, NULL, NULL);
	while (!font && (fn && *fn))
	{
		font = XLoadQueryFont(dpy, fn);
		if (fn != NULL)
		{
			free(fn);
			fn = NULL;
		}
		if (!font && str && *str)
		{
			str = GetQuotedString(str, &fn, ",", NULL, NULL, NULL);
		}
	}
	if (font == NULL)
	{
		if (fn != NULL)
			free(fn);
		if (tmp != NULL)
			free(tmp);
		return NULL;
	}

	flf = (FlocaleFont *)safemalloc(sizeof(FlocaleFont));
	memset(flf, '\0', sizeof(FlocaleFont));
	flf->count = 1;
	flf->fontset = None;
	flf->fftf.fftfont = NULL;
	flf->font = font;
	FlocaleCharsetSetFlocaleCharset(dpy, flf, hints, encoding, module);
	flf->height = flf->font->ascent + flf->font->descent;
	flf->ascent = flf->font->ascent;
	flf->descent = flf->font->descent;
	flf->max_char_width = flf->font->max_bounds.width;
	if (flf->font->max_byte1 > 0)
		flf->flags.is_mb = True;
	if (fn != NULL)
		free(fn);
	if (tmp != NULL)
		free(tmp);

	return flf;
}

static
FlocaleFont *FlocaleGetFontOrFontSet(
	Display *dpy, char *fontname, char *encoding, char *fullname,
	char *module)
{
	FlocaleFont *flf = NULL;

	if (fontname && strlen(fontname) > 4 &&
	    strncasecmp("xft:", fontname, 4) == 0)
	{
		if (FftSupport)
		{
			flf = FlocaleGetFftFont(
				dpy, fontname+4, encoding, module);
		}
		if (flf)
		{
			CopyString(&flf->name, fullname);
		}
		return flf;
	}
	if (flf == NULL && Flocale != NULL && fontname)
	{
		flf = FlocaleGetFontSet(dpy, fontname, encoding, module);
	}
	if (flf == NULL && fontname)
	{
		flf = FlocaleGetFont(dpy, fontname, encoding, module);
	}
	if (flf && fontname)
	{
		if (StrEquals(fullname, FLOCALE_MB_FALLBACK_FONT))
		{
			flf->name = FLOCALE_MB_FALLBACK_FONT;
		}
		else if (StrEquals(fullname, FLOCALE_FALLBACK_FONT))
		{
			flf->name = FLOCALE_FALLBACK_FONT;
		}
		else
		{
			CopyString(&flf->name, fullname);
		}
		return flf;
	}
	return NULL;
}

/* ***************************************************************************
 * locale local functions
 * ***************************************************************************/

static
void FlocaleSetlocaleForX(
	int category, const char *locale, const char *module)
{
	if ((Flocale = setlocale(category, locale)) == NULL)
	{
		fprintf(stderr,
			"[%s][%s]: ERROR -- Cannot set locale. Please check"
			" your $LC_CTYPE or $LANG.\n",
			(module == NULL)? "" : module, "FlocaleSetlocaleForX");
		return;
	}
	if (!XSupportsLocale())
	{
		fprintf(stderr,
			"[%s][%s]: WARNING -- X does not support locale %s\n",
			(module == NULL)? "": module, "FlocaleSetlocaleForX",
			Flocale);
		Flocale = NULL;
	}
}

/* ---------------------------- interface functions ------------------------- */

/* ***************************************************************************
 * locale initialisation
 * ***************************************************************************/

void FlocaleInit(
	int category, const char *locale, const char *modifiers,
	const char *module)
{

	FlocaleSetlocaleForX(category, locale, module);
	if (Flocale == NULL)
		return;

	if (modifiers != NULL &&
	    (Fmodifiers = XSetLocaleModifiers(modifiers)) == NULL)
	{
		fprintf(stderr,
			"[%s][%s]: WARNING -- Cannot set locale modifiers\n",
			(module == NULL)? "": module, "FlocaleInit");
	}
#if FLOCALE_DEBUG_SETLOCALE
	fprintf(stderr,"[%s][FlocaleInit] locale: %s, modifier: %s\n",
		module, Flocale, Fmodifiers);
#endif
}

/* ***************************************************************************
 * fonts loading
 * ***************************************************************************/
char *prefix_list[] =
{
	"Shadow=",
	"StringEncoding=",
	NULL
};

FlocaleFont *FlocaleLoadFont(Display *dpy, char *fontname, char *module)
{
	FlocaleFont *flf = FlocaleFontList;
	Bool ask_default = False;
	char *t;
	char *str, *opt_str, *encoding= NULL, *fn = NULL;
	int shadow_size = 0;
	int shadow_offset = 0;
	int shadow_dir;
	int i;

	/* removing quoting for modules */
	if (fontname && (t = strchr("\"'`", *fontname)))
	{
		char c = *t;
		fontname++;
		if (fontname[strlen(fontname)-1] == c)
			fontname[strlen(fontname)-1] = 0;
	}

	if (fontname == NULL || *fontname == 0)
	{
		ask_default = True;
		fontname = FLOCALE_MB_FALLBACK_FONT;
	}

	while (flf)
	{
		char *c1, *c2;

		for (c1 = fontname, c2 = flf->name; *c1 && *c2; ++c1, ++c2)
		{
			if (*c1 != *c2)
			{
				break;
			}
		}
		if (!*c1 && !*c2)
		{
			flf->count++;
			return flf;
		}
		flf = flf->next;
	}

	/* not cached load the font as a ";" separated list */

	/* But first see if we have a shadow relief and/or an encoding */
	str = fontname;
	while ((i = GetTokenIndex(str, prefix_list, -1, &str)) > -1)
	{
		str = GetQuotedString(str, &opt_str, ":", NULL, NULL, NULL);
		switch(i)
		{
		case 0: /* shadow= */
			FlocaleParseShadow(
				opt_str, &shadow_size, &shadow_offset,
				&shadow_dir, fontname, module);
			break;
		case 1: /* encoding= */
			if (encoding != NULL)
			{
				free(encoding);
				encoding = NULL;
			}
			if (opt_str && *opt_str)
			{
				CopyString(&encoding, opt_str);
			}
			break;
		default:
			break;
		}
		if (opt_str != NULL)
			free(opt_str);
	}
	if (str && *str)
	{
		str = GetQuotedString(str, &fn, ";", NULL, NULL, NULL);
	}
	else
	{
		fn = FLOCALE_MB_FALLBACK_FONT;
	}
	while (!flf && (fn && *fn))
	{
		flf = FlocaleGetFontOrFontSet(
			dpy, fn, encoding, fontname, module);
		if (fn != NULL && fn != FLOCALE_MB_FALLBACK_FONT &&
		    fn != FLOCALE_FALLBACK_FONT)
		{
			free(fn);
			fn = NULL;
		}
		if (!flf && str && *str)
		{
			str = GetQuotedString(str, &fn, ";", NULL, NULL, NULL);
		}
	}
	if (fn != NULL && fn != FLOCALE_MB_FALLBACK_FONT &&
	    fn != FLOCALE_FALLBACK_FONT)
	{
		free(fn);
	}

	if (flf == NULL)
	{
		/* loading failed, try default font */
		if (!ask_default)
		{
			fprintf(stderr,"[%s][FlocaleLoadFont]: "
				"WARNING -- can't load font '%s',"
				" trying default:\n",
				(module)? module: "FVWMlibs", fontname);
		}
		else
		{
			/* we already tried default fonts: try again? yes */
		}
		if (Flocale != NULL)
		{
			if (!ask_default)
			{
				fprintf(stderr, "\t%s\n",
					FLOCALE_MB_FALLBACK_FONT);
			}
			if ((flf = FlocaleGetFontSet(
				dpy, FLOCALE_MB_FALLBACK_FONT, NULL,
				module)) != NULL)
			{
				flf->name = FLOCALE_MB_FALLBACK_FONT;
			}
		}
		if (flf == NULL)
		{
			if (!ask_default)
			{
				fprintf(stderr,"\t%s\n",
					FLOCALE_FALLBACK_FONT);
			}
			if ((flf =
			     FlocaleGetFont(
			       dpy, FLOCALE_FALLBACK_FONT, NULL, module)) !=
			    NULL)
			{
				flf->name = FLOCALE_FALLBACK_FONT;
			}
			else if (!ask_default)
			{
				fprintf(stderr,
					"[%s][FlocaleLoadFont]:"
					" ERROR -- can't load font.\n",
					(module)? module: "FVWMlibs");
			}
			else
			{
				fprintf(stderr,
					"[%s][FlocaleLoadFont]: ERROR"
					" -- can't load default font:\n",
					(module)? module: "FVWMlibs");
				fprintf(stderr, "\t%s\n",
					FLOCALE_MB_FALLBACK_FONT);
				fprintf(stderr, "\t%s\n",
					FLOCALE_FALLBACK_FONT);
			}
		}
	}

	if (flf != NULL)
	{
		if (shadow_size > 0)
		{
			flf->shadow_size = shadow_size;
			flf->flags.shadow_dir = shadow_dir;
			flf->shadow_offset = shadow_offset;
			flf->descent += FLF_SHADOW_DESCENT(flf);
			flf->ascent += FLF_SHADOW_ASCENT(flf);
			flf->height += FLF_SHADOW_HEIGHT(flf);
			flf->max_char_width += FLF_SHADOW_WIDTH(flf);
		}
		if (flf->fc == FlocaleCharsetGetUnknownCharset())
		{
			fprintf(stderr,"[%s][FlocaleLoadFont]: "
				"WARNING -- Unkown charset for font\n\t'%s'\n",
				(module)? module: "FVWMlibs", flf->name);
			flf->fc = FlocaleCharsetGetDefaultCharset(dpy, module);
		}
		else if (flf->str_fc == FlocaleCharsetGetUnknownCharset() &&
			 (encoding != NULL ||
			  (FftSupport && flf->fftf.fftfont != NULL &&
			   flf->fftf.str_encoding != NULL)))
		{
			fprintf(stderr,"[%s][FlocaleLoadFont]: "
				"WARNING -- Unkown string encoding for font\n"
				"\t'%s'\n",
				(module)? module: "FVWMlibs", flf->name);
		}
		if (flf->str_fc == FlocaleCharsetGetUnknownCharset())
		{
			flf->str_fc =
				FlocaleCharsetGetDefaultCharset(dpy, module);
		}
		flf->next = FlocaleFontList;
		FlocaleFontList = flf;
	}
	if (encoding != NULL)
	{
		free(encoding);
	}

	return flf;
}

void FlocaleUnloadFont(Display *dpy, FlocaleFont *flf)
{
	FlocaleFont *list = FlocaleFontList;
	int i = 0;

	if (!flf)
	{
		return;
	}
	/* Remove a weight, still too heavy? */
	if (--(flf->count) > 0)
	{
		return;
	}

	if (flf->name != NULL &&
		!StrEquals(flf->name, FLOCALE_MB_FALLBACK_FONT) &&
		!StrEquals(flf->name, FLOCALE_FALLBACK_FONT))
	{
		free(flf->name);
	}
	if (FftSupport && flf->fftf.fftfont != NULL)
	{
		FftFontClose(dpy, flf->fftf.fftfont);
		if (flf->fftf.fftfont_rotated_90 != NULL)
			FftFontClose(dpy, flf->fftf.fftfont_rotated_90);
		if (flf->fftf.fftfont_rotated_180 != NULL)
			FftFontClose(dpy, flf->fftf.fftfont_rotated_180);
		if (flf->fftf.fftfont_rotated_270 != NULL)
			FftFontClose(dpy, flf->fftf.fftfont_rotated_270);
	}
	if (flf->fontset != NULL)
	{
		XFreeFontSet(dpy, flf->fontset);
	}
	if (flf->font != NULL)
	{
		XFreeFont(dpy, flf->font);
	}
	if (flf->flags.must_free_fc)
	{
		if (flf->fc->x)
			free(flf->fc->x);
		if (flf->fc->bidi)
			free(flf->fc->bidi);
		if (flf->fc->locale != NULL)
		{
			while (FLC_GET_LOCALE_CHARSET(flf->fc,i) != NULL)
			{
				free(FLC_GET_LOCALE_CHARSET(flf->fc,i));
				i++;
			}
			free(flf->fc->locale);
		}
		free(flf->fc);
	}

	/* Link it out of the list (it might not be there) */
	if (flf == list) /* in head? simple */
	{
		FlocaleFontList = flf->next;
	}
	else
	{
		while (list && list->next != flf)
		{
			/* fast forward until end or found */
			list = list->next;
		}
		/* not end? means we found it in there, possibly at end */
		if (list)
		{
			/* link around it */
			list->next = flf->next;
		}
	}
	free(flf);
}

/* ***************************************************************************
 * Width and Drawing Text
 * ***************************************************************************/

void FlocaleInitGstpArgs(
	flocale_gstp_args *args, FlocaleFont *flf, FlocaleWinString *fws,
	int start_x, int start_y)
{
	args->step = 0;
	args->offset = flf->shadow_offset + 1;
	args->outer_offset = flf->shadow_offset + flf->shadow_size;
	args->size = flf->shadow_size;
	args->sdir = flf->flags.shadow_dir;
	switch (fws->flags.text_rotation)
	{
	case ROTATION_270: /* CCW */
		args->orig_x = start_x + FLF_SHADOW_UPPER_SIZE(flf);
		args->orig_y = start_y + FLF_SHADOW_RIGHT_SIZE(flf);
		break;
	case ROTATION_180:
		args->orig_x = start_x + FLF_SHADOW_RIGHT_SIZE(flf);
		args->orig_y = start_y;
		break;
	case ROTATION_90: /* CW */
		args->orig_x = start_x + FLF_SHADOW_BOTTOM_SIZE(flf);
		args->orig_y = start_y + FLF_SHADOW_LEFT_SIZE(flf);
		break;
	case ROTATION_0:
	default:
		args->orig_x = start_x + FLF_SHADOW_LEFT_SIZE(flf);
		args->orig_y = start_y;
		break;
	}
	args->rot = fws->flags.text_rotation;

	return;
}

Bool FlocaleGetShadowTextPosition(
	int *x, int *y, flocale_gstp_args *args)
{
	if (args->step == 0)
	{
		args->direction = MULTI_DIR_NONE;
		args->inter_step = 0;
	}
	if ((args->step == 0 || args->inter_step >= args->num_inter_steps) &&
	    args->size != 0)
	{
		/* setup a new direction */
		args->inter_step = 0;
		gravity_get_next_multi_dir(args->sdir, &args->direction);
		if (args->direction == MULTI_DIR_C)
		{
			int size;

			size = 2 * (args->outer_offset) + 1;
			args->num_inter_steps = size * size;
		}
		else
		{
			args->num_inter_steps = args->size;
		}
	}
	if (args->direction == MULTI_DIR_NONE || args->size == 0)
	{
		*x = args->orig_x;
		*y = args->orig_y;

		return False;
	}
	if (args->direction == MULTI_DIR_C)
	{
		int tx;
		int ty;
		int size;
		int is_finished;

		size = 2 * (args->outer_offset) + 1;
		tx = args->inter_step % size - args->outer_offset;
		ty = args->inter_step / size - args->outer_offset;
		for (is_finished = 0; ty <= args->outer_offset;
		     ty++, tx = -args->outer_offset)
		{
			for (; tx <= args->outer_offset; tx++)
			{
				if (tx <= -args->offset || tx >= args->offset ||
				    ty <= -args->offset || ty >= args->offset)
				{
					is_finished = 1;
					break;
				}
			}
			if (is_finished)
			{
				break;
			}
		}
		args->inter_step =
			(tx + args->outer_offset) +
			(ty + args->outer_offset) * size;
		if (!is_finished)
		{
			tx = 0;
			ty = 0;
		}
		*x = args->orig_x + tx;
		*y = args->orig_y + ty;
	}
	else if (args->inter_step > 0)
	{
		/* into a directional drawing */
		(*x) += args->x_sign;
		(*y) += args->y_sign;
	}
	else
	{
		direction_type dir;
		direction_type dir_x;
		direction_type dir_y;

		dir = gravity_multi_dir_to_dir(args->direction);
		gravity_split_xy_dir(&dir_x, &dir_y, dir);
		args->x_sign = gravity_dir_to_sign_one_axis(dir_x);
		args->y_sign = gravity_dir_to_sign_one_axis(dir_y);
		gravity_rotate_xy(
			args->rot, args->x_sign, args->y_sign,
			&args->x_sign, &args->y_sign);
		*x = args->orig_x + args->x_sign * args->offset;
		*y = args->orig_y + args->y_sign * args->offset;
	}
	args->inter_step++;
	args->step++;

	return True;
}

void FlocaleDrawString(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws,
	unsigned long flags)
{
	int len;
	Bool do_free = False;
	Pixel fg = 0, fgsh = 0;
	Bool has_fg_pixels = False;
	flocale_gstp_args gstp_args;

	if (!fws || !fws->str)
	{
		return;
	}

	if (flags & FWS_HAVE_LENGTH)
	{
		len = fws->len;
	}
	else
	{
		len = strlen(fws->str);
	}

	/* encode the string */
	FlocaleEncodeWinString(dpy, flf, fws, &do_free, &len);

	/* get the pixels */
	if (fws->flags.has_colorset)
	{
		fg = fws->colorset->fg;
		fgsh = fws->colorset->fgsh;
		has_fg_pixels = True;
	}

	if (fws->flags.text_rotation != ROTATION_0 &&
	    flf->fftf.fftfont == NULL)
	{
		FlocaleRotateDrawString(
			dpy, flf, fws, fg, fgsh, has_fg_pixels, len);
	}
	else if (FftSupport && flf->fftf.fftfont != NULL)
	{
		FftDrawString(
			dpy, flf, fws, fg, fgsh, has_fg_pixels, len, flags);
	}
	else if (flf->fontset != None)
	{
		int xt = fws->x;
		int yt = fws->y;

		FlocaleInitGstpArgs(&gstp_args, flf, fws, fws->x, fws->y);
		if (flf->shadow_size != 0 && has_fg_pixels)
		{
			XSetForeground(dpy, fws->gc, fgsh);
			while (FlocaleGetShadowTextPosition(
				       &xt, &yt, &gstp_args))
			{
				XmbDrawString(
					dpy, fws->win, flf->fontset, fws->gc,
					xt, yt, fws->e_str, len);
			}
			XSetForeground(dpy, fws->gc, fg);
		}
		xt = gstp_args.orig_x;
		yt = gstp_args.orig_y;
		XmbDrawString(
			dpy, fws->win, flf->fontset, fws->gc,
			xt, yt, fws->e_str, len);
	}
	else if (flf->font != None)
	{
		FlocaleFontStructDrawString(
			dpy, flf, fws->win, fws->gc, fws->x, fws->y,
			fg, fgsh, has_fg_pixels, fws, len, False);
	}

	if (do_free)
	{
		if (fws->e_str != NULL)
		{
			free(fws->e_str);
			fws->e_str = NULL;
		}
		if (fws->str2b != NULL)
		{
			free(fws->str2b);
			fws->str2b = NULL;
		}
	}

	return;
}

void FlocaleDrawUnderline(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws, int coffset)
{
	int off1, off2, y, x_s, x_e;

	off1 = FlocaleTextWidth(flf, fws->str, coffset) +
		((coffset == 0)?
		 FLF_SHADOW_LEFT_SIZE(flf) : - FLF_SHADOW_RIGHT_SIZE(flf) );
	off2 = FlocaleTextWidth(flf, fws->str + coffset, 1) -
		FLF_SHADOW_WIDTH(flf) - 1 + off1;
	y = fws->y + 2;
	x_s = fws->x + off1;
	x_e = fws->x + off2;

	/* No shadow */
	XDrawLine(dpy, fws->win, fws->gc, x_s, y, x_e, y);

	return;
}

int FlocaleTextWidth(FlocaleFont *flf, char *str, int sl)
{
	int result = 0;
	char *tmp_str;
	int new_l,do_free;

	if (!str || sl == 0)
		return 0;

	if (sl < 0)
	{
		/* a vertical string: nothing to do! */
		sl = -sl;
	}
	/* FIXME */
	tmp_str = FlocaleEncodeString(
		Pdpy, flf, str, &do_free, sl, &new_l, NULL);
	if (FftSupport && flf->fftf.fftfont != NULL)
	{
		result = FftTextWidth(flf, tmp_str, new_l);
	}
	else if (flf->fontset != None)
	{
		result = XmbTextEscapement(flf->fontset, tmp_str, new_l);
	}
	else if (flf->font != None)
	{
		if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc) || flf->flags.is_mb)
		{
			XChar2b *str2b;
			int nl;

			if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
				str2b = FlocaleUtf8ToUnicodeStr2b(
							tmp_str, new_l, &nl);
			else
				str2b = FlocaleStringToString2b(
						Pdpy, flf, tmp_str, new_l, &nl);
			if (str2b != NULL)
			{
				result = XTextWidth16(flf->font, str2b, nl);
				free(str2b);
			}
		}
		else
		{
			result = XTextWidth(flf->font, tmp_str, new_l);
		}
	}
	if (do_free)
	{
		free(tmp_str);
	}
	return result + ((result != 0)? FLF_SHADOW_WIDTH(flf):0);
}

void FlocaleAllocateWinString(FlocaleWinString **pfws)
{
	*pfws = (FlocaleWinString *)safemalloc(sizeof(FlocaleWinString));
	memset(*pfws, '\0', sizeof(FlocaleWinString));
}

/* ***************************************************************************
 * Text properties
 * ***************************************************************************/
void FlocaleGetNameProperty(
	Status (func)(Display *, Window, XTextProperty *), Display *dpy,
	Window w, FlocaleNameString *ret_name)
{
	char **list;
	int num;
	XTextProperty text_prop;

	if (func(dpy, w, &text_prop) == 0)
	{
		return;
	}

	if (text_prop.encoding == XA_STRING)
	{
		/* STRING encoding, use this as it is */
		ret_name->name = (char *)text_prop.value;
		ret_name->name_list = NULL;
		return;
	}
	/* not STRING encoding, try to convert XA_COMPOUND_TEXT */
	if (XmbTextPropertyToTextList(dpy, &text_prop, &list, &num) >= Success
	    && num > 0 && *list)
	{
		/* Does not consider the conversion is REALLY succeeded:
		 * XmbTextPropertyToTextList return 0 (== Success) on success,
		 * a negative int if it fails (and in this case we are not here),
		 * the number of unconvertible char on "partial" success*/
		XFree(text_prop.value); /* return of XGetWM(Icon)Name() */
		ret_name->name = *list;
		ret_name->name_list = list;
	}
	else
	{
		if (list)
		{
			XFreeStringList(list);
		}
		ret_name->name = (char *)text_prop.value;
		ret_name->name_list = NULL;
	}
}

void FlocaleFreeNameProperty(FlocaleNameString *ptext)
{
	if (ptext->name_list != NULL)
	{
		if (ptext->name != NULL && ptext->name != *ptext->name_list)
			XFree(ptext->name);
		XFreeStringList(ptext->name_list);
		ptext->name_list = NULL;
	}
	else if (ptext->name != NULL)
	{
		XFree(ptext->name);
	}
	ptext->name = NULL;

	return;
}

Bool FlocaleTextListToTextProperty(
	Display *dpy, char **list, int count, XICCEncodingStyle style,
	XTextProperty *text_prop_return)
{
	int ret = False;

	if (Flocale != NULL)
	{
		ret = XmbTextListToTextProperty(
			dpy, list, count, style, text_prop_return);
		if (ret == XNoMemory)
		{
			ret = False;
		}
		else
		{
			/* ret == Success or the number of unconvertible
			 * characters. ret should be != XLocaleNotSupported
			 * because in this case Flocale == NULL */
			ret = True;
		}
	}
	if (!ret)
	{
		if (XStringListToTextProperty(
			    list, count, text_prop_return) == 0)
		{
			ret = False;
		}
		else
		{
			ret = True;
		}
	}

	return ret;
}
