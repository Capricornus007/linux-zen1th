/*
 *  linux/drivers/video/console/fbcon_rotate.c -- Software Rotation
 *
 *      Copyright (C) 2005 Antonino Daplas <adaplas @pol.net>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/font.h>
#include <linux/overflow.h>
#include <linux/slab.h>

#include "fbcon.h"
#include "fbcon_rotate.h"

static void fbcon_rotate_font_utf(struct fb_info *info, struct vc_data *vc)
{
	struct fbcon_par *par = info->fbcon_par;
	unsigned int cellsize = font_glyph_size(vc->vc_font.width, vc->vc_font.height);
	const char *fontname = (cellsize < 64) ? "CJK16x16" : "CJK32x32";
	const struct font_desc *font_cjk = find_font(fontname);
	unsigned char *buf;

	if (!font_cjk || !font_cjk->data)
		return;

	par->rotated.fontdata_utf = font_cjk->data;
	buf = font_data_rotate(font_cjk->data,
			       font_cjk->width, font_cjk->height,
			       font_cjk->charcount,
			       par->rotated.buf_rotate,
			       par->rotated.buf_utf,
			       &par->rotated.bufsize_utf);
	if (!IS_ERR(buf))
		par->rotated.buf_utf = buf;
}

int fbcon_rotate_font(struct fb_info *info, struct vc_data *vc)
{
	struct fbcon_par *par = info->fbcon_par;
	unsigned char *buf;
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
			       par->rotated.buf_rotate, par->rotated.buf,
			       &par->rotated.bufsize);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_kfree;
	}

	par->rotated.buf = buf;
	fbcon_rotate_font_utf(info, vc);

	return 0;

err_kfree:
	kfree(par->rotated.buf);
	par->rotated.buf = NULL; /* clear here to avoid output */
	par->rotated.bufsize = 0;

	return ret;
}
