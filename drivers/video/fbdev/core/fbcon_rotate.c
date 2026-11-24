/*
 *  linux/drivers/video/console/fbcon_rotate.c -- Software Rotation
 *
 *      Copyright (C) 2005 Antonino Daplas <adaplas @pol.net>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 *
 *  cjktty-next adaptation 2026-11-24:
 *  linux-next reworked rotated font buffers to font_data_t (refcounted, size
 *  in a hidden header). cjktty's vanilla-rc4 code used plain kvmalloc +
 *  explicit bufsize. This file keeps next as the single source of rotated
 *  buffers (font_data_rotate) and adds a CJK-only shadow buffer (buf_utf)
 *  for the CJK font's rotated glyphs.
 */

#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/font.h>
#ifdef CONFIG_FONT_CJK
#include <linux/slab.h>
#endif

#include "fbcon.h"
#include "fbcon_rotate.h"

#ifdef CONFIG_FONT_CJK
/* Rotate the CJK font into a shadow buffer; font_bits() consults it. */
static int fbcon_rotate_font_utf(struct fb_info *info, struct vc_data *vc)
{
	struct fbcon_par *par = info->fbcon_par;
	const char *fontname = vc->vc_font.width == 8 && vc->vc_font.height == 16 ?
		"CJK16x16" : vc->vc_font.width == 16 && vc->vc_font.height == 32 ?
		"CJK32x32" : NULL;
	const struct font_desc *font_cjk = fontname ? find_font(fontname) : NULL;
	font_data_t *buf;
	int ret;

	par->rotated.fontdata_utf = NULL;
	if (!font_cjk || !font_cjk->data || font_cjk->data == vc->vc_font.data)
		return 0;

	/* next's font_data_rotate() allocates/grows via its own refcounted
	 * buffer; pass the previous one so it can be reused or replaced. */
	buf = font_data_rotate(font_cjk->data, font_cjk->width,
			       font_cjk->height, font_cjk->charcount,
			       par->rotated.buf_rotate, par->rotated.buf_utf);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto invalidate;
	}
	if (par->rotated.buf_utf)
		font_data_put(par->rotated.buf_utf);
	par->rotated.buf_utf = buf;
	par->rotated.bufsize_utf = font_data_size(buf);
	par->rotated.fontdata_utf = font_cjk->data;
	return 0;

invalidate:
	if (par->rotated.buf_utf)
		font_data_put(par->rotated.buf_utf);
	par->rotated.buf_utf = NULL;
	par->rotated.bufsize_utf = 0;
	par->rotated.fontdata_utf = NULL;
	return ret;
}
#endif

int fbcon_rotate_font(struct fb_info *info, struct vc_data *vc)
{
	struct fbcon_par *par = info->fbcon_par;
	font_data_t *buf;
	int ret;

	if (par->p->fontdata == par->rotated.fontdata &&
	    par->rotate == par->rotated.buf_rotate &&
	    vc->vc_font.width == par->rotated.buf_width &&
	    vc->vc_font.height == par->rotated.buf_height &&
	    vc->vc_font.charcount == par->rotated.buf_charcount &&
	    par->rotated.buf)
		return 0;

	par->rotated.fontdata = par->p->fontdata;
	par->rotated.buf_rotate = par->rotate;
	par->rotated.buf_width = vc->vc_font.width;
	par->rotated.buf_height = vc->vc_font.height;
	par->rotated.buf_charcount = vc->vc_font.charcount;

	if (info->fbops->fb_sync)
		info->fbops->fb_sync(info);

	buf = font_data_rotate(par->rotated.fontdata, vc->vc_font.width,
			       vc->vc_font.height, vc->vc_font.charcount,
			       par->rotated.buf_rotate, par->rotated.buf);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_font_data_put;
	}

	if (par->rotated.buf)
		font_data_put(par->rotated.buf);
	par->rotated.buf = buf;
#ifdef CONFIG_FONT_CJK
	par->rotated.bufsize = font_data_size(buf);
	ret = fbcon_rotate_font_utf(info, vc);
	if (ret)
		goto err_font_data_put;
#endif
	return 0;

err_font_data_put:
	if (par->rotated.buf) {
		font_data_put(par->rotated.buf);
		par->rotated.buf = NULL; /* clear here to avoid output */
	}
	return ret;
}
