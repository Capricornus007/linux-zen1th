/*
 *  linux/drivers/video/console/bitblit.c -- BitBlitting Operation
 *
 *  Originally from the 'accel_*' routines in drivers/video/console/fbcon.c
 *
 *      Copyright (C) 2004 Antonino Daplas <adaplas @pol.net>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/console.h>
#ifdef CONFIG_FONT_CJK
#include <linux/font.h>
#endif
#include <asm/types.h>
#include "fbcon.h"
#include "fbcon_rotate.h"

/*
 * Accelerated handlers.
 */
static void update_attr(unsigned char *dst, const unsigned char *src, int attribute,
			struct vc_data *vc)
{
	int i, offset = (vc->vc_font.height < 10) ? 1 : 2;
	int width = DIV_ROUND_UP(vc->vc_font.width, 8);
	unsigned int cellsize = vc->vc_font.height * width;
	unsigned char c;

	offset = cellsize - (offset * width);
	for (i = 0; i < cellsize; i++) {
		c = src[i];
		if (attribute & FBCON_ATTRIBUTE_UNDERLINE && i >= offset)
			c = 0xff;
		if (attribute & FBCON_ATTRIBUTE_BOLD)
			c |= c >> 1;
		if (attribute & FBCON_ATTRIBUTE_REVERSE)
			c = ~c;
		dst[i] = c;
	}
}

#ifdef CONFIG_FONT_CJK
const u8 *font_bits(struct vc_data *vc, u16 ch, u32 c_utf, bool continuation,
		    u32 cellsize, struct fbcon_par *par)
{
	const char *fontname = vc->vc_font.width == 8 && vc->vc_font.height == 16 ?
		"CJK16x16" : vc->vc_font.width == 16 && vc->vc_font.height == 32 ?
		"CJK32x32" : NULL;

	if (c_utf > 0xffff)
		c_utf = 0xfffd;

#ifdef CONFIG_FRAMEBUFFER_CONSOLE_ROTATION
	if (par) {
		static const u8 blank[DIV_ROUND_UP(FB_MAX_BLIT_WIDTH, 8) *
				      FB_MAX_BLIT_HEIGHT];
		const struct font_desc *font;
		const u8 *fontdata;
		u32 cellsize_utf;
		size_t offset, size;

		/* rotated path: lookup in the rotated glyph buffer */
		if (!par->rotated.buf || par->rotated.bufsize < cellsize ||
		    par->rotated.fontdata != vc->vc_font.data)
			return blank;
		if (!c_utf) {
			if (ch >= vc->vc_font.charcount ||
			    ((size_t)ch + 1) * cellsize > par->rotated.bufsize)
				ch = 0;
			return par->rotated.buf + ch * cellsize;
		}

		/* CJK characters: lookup in the UTF rotated buffer */
		font = fontname ? find_font(fontname) : NULL;
		if (!font || !font->data)
			return par->rotated.buf;
		if (font->data == vc->vc_font.data) {
			fontdata = par->rotated.buf;
			size = par->rotated.bufsize;
		} else {
			if (!par->rotated.buf_utf ||
			    par->rotated.fontdata_utf != font->data)
				return par->rotated.buf;
			fontdata = par->rotated.buf_utf;
			size = par->rotated.bufsize_utf;
		}

		cellsize_utf = par->rotated.buf_rotate % 2 ?
			font_glyph_size(font->height, font->width) :
			font_glyph_size(font->width, font->height);
		offset = (size_t)c_utf * 2 + continuation;
		if (cellsize != cellsize_utf || offset >= font->charcount)
			return par->rotated.buf;
		offset *= cellsize_utf;
		if (offset + cellsize > size)
			return par->rotated.buf;
		return fontdata + offset;
	} else
#endif
	{
		const struct font_desc *font;
		const u8 *fontdata;
		u32 cellsize_utf;
		size_t offset;

		/* non-rotated path (bitblit): lookup in font data directly */
		if (!c_utf) {
			if (ch >= vc->vc_font.charcount)
				ch = 0;
			return vc->vc_font.data + ch * cellsize;
		}

		font = fontname ? find_font(fontname) : NULL;
		if (!font || !font->data)
			return vc->vc_font.data;

		cellsize_utf = font_glyph_size(font->width, font->height);
		offset = (size_t)c_utf * 2 + continuation;
		if (cellsize != cellsize_utf || offset >= font->charcount)
			return vc->vc_font.data;
		fontdata = font_data_buf(font->data);
		offset *= cellsize_utf;
		if (offset + cellsize > font_data_size(font->data))
			return vc->vc_font.data;
		return fontdata + offset;
	}
}
#endif

static void bit_bmove(struct vc_data *vc, struct fb_info *info, int sy,
		      int sx, int dy, int dx, int height, int width)
{
	struct fb_copyarea area;

	area.sx = sx * vc->vc_font.width;
	area.sy = sy * vc->vc_font.height;
	area.dx = dx * vc->vc_font.width;
	area.dy = dy * vc->vc_font.height;
	area.height = height * vc->vc_font.height;
	area.width = width * vc->vc_font.width;

	info->fbops->fb_copyarea(info, &area);
}

static void bit_clear(struct vc_data *vc, struct fb_info *info, int sy,
		      int sx, int height, int width, int fg, int bg)
{
	struct fb_fillrect region;

	region.color = bg;
	region.dx = sx * vc->vc_font.width;
	region.dy = sy * vc->vc_font.height;
	region.width = width * vc->vc_font.width;
	region.height = height * vc->vc_font.height;
	region.rop = ROP_COPY;

	info->fbops->fb_fillrect(info, &region);
}

static inline void bit_putcs_aligned(struct vc_data *vc, struct fb_info *info,
				     const u16 *s, u32 attr, u32 cnt,
				     u32 d_pitch, u32 s_width, u32 s_vpitch,
				     struct fb_image *image,
				     unsigned char *buf, u8 *dst)
{
	u16 charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	u32 idx = vc->vc_font.width >> 3;
	font_data_t *fd = (font_data_t *)vc->vc_font.data; /* upcast to font_data_t */
	u32 s_pitch = font_glyph_pitch(s_width);
	const unsigned char *src;

	while (cnt--) {
		u16 ch = scr_readw(s++) & charmask;

#ifdef CONFIG_FONT_CJK
		bool continuation;
		u32 cy = (image->dy / vc->vc_font.height + par->p->vrows -
			  par->p->yscroll) % par->p->vrows;
		u32 cx = image->dx / vc->vc_font.width + (cnt);
		u32 c_utf = vc_uniscr_get_cjk(vc, cy, cx, &continuation);
		u32 cellsize = font_glyph_size(s_width, s_vpitch);

		src = font_bits(vc, ch, c_utf, continuation, cellsize, NULL);
#else
		src = font_data_glyph_buf(fd, s_width, s_vpitch, ch);
#endif
		if (!src) {
			src = font_data_glyph_buf(fd, s_width, s_vpitch, '\0');
			if (!src)
				goto next_char;
		}

		if (attr) {
			update_attr(buf, src, attr, vc);
			src = buf;
		}

		if (likely(idx == 1))
			__fb_pad_aligned_buffer(dst, d_pitch, src, idx,
						image->height);
		else
			fb_pad_aligned_buffer(dst, d_pitch, src, idx,
					      image->height);
next_char:
		dst += s_pitch;
	}

	info->fbops->fb_imageblit(info, image);
}

static inline void bit_putcs_unaligned(struct vc_data *vc,
				       struct fb_info *info, const u16 *s,
				       u32 attr, u32 cnt, u32 d_pitch,
				       u32 s_width, u32 s_vpitch,
				       struct fb_image *image,
				       unsigned char *buf, u8 *dst)
{
	u16 charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	u32 shift_low = 0, mod = vc->vc_font.width % 8;
	u32 shift_high = 8;
	u32 idx = vc->vc_font.width >> 3;
	font_data_t *fd = (font_data_t *)vc->vc_font.data; /* upcast to font_data_t */
	u32 s_pitch = font_glyph_pitch(s_width);
	const unsigned char *src;

	while (cnt--) {
		u16 ch = scr_readw(s++) & charmask;

#ifdef CONFIG_FONT_CJK
		bool continuation;
		u32 cy = (image->dy / vc->vc_font.height + par->p->vrows -
			  par->p->yscroll) % par->p->vrows;
		u32 cx = image->dx / vc->vc_font.width + (cnt);
		u32 c_utf = vc_uniscr_get_cjk(vc, cy, cx, &continuation);
		u32 cellsize = font_glyph_size(s_width, s_vpitch);

		src = font_bits(vc, ch, c_utf, continuation, cellsize, NULL);
#else
		src = font_data_glyph_buf(fd, s_width, s_vpitch, ch);
#endif
		if (!src) {
			src = font_data_glyph_buf(fd, s_width, s_vpitch, '\0');
			if (!src)
				goto next_char;
		}

		if (attr) {
			update_attr(buf, src, attr, vc);
			src = buf;
		}

		fb_pad_unaligned_buffer(dst, d_pitch, src, idx,
					image->height, shift_high,
					shift_low, mod);
next_char:
		shift_low += mod;
		dst += (shift_low >= 8) ? s_pitch : s_pitch - 1;
		shift_low &= 7;
		shift_high = 8 - shift_low;
	}

	info->fbops->fb_imageblit(info, image);

}

static void bit_putcs(struct vc_data *vc, struct fb_info *info,
		      const unsigned short *s, int count, int yy, int xx,
		      int fg, int bg)
{
	struct fbcon_par *par = info->fbcon_par;
	struct fb_image image;
	u32 vyres = GETVYRES(par->p, info);
	u32 width = vc->vc_font.width;
	u32 vpitch = vc->vc_font.height;
	u32 cellsize = font_glyph_size(width, vpitch);
	u32 maxcnt = info->pixmap.size/cellsize;
	u32 scan_align = info->pixmap.scan_align - 1;
	u32 buf_align = info->pixmap.buf_align - 1;
	u32 mod = vc->vc_font.width % 8, cnt, pitch, size;
	u32 attribute = get_attribute(info, scr_readw(s));
	unsigned char *buf = NULL;
	u8 *dst;

	image.fg_color = fg;
	image.bg_color = bg;
	image.dx = xx * vc->vc_font.width;
	image.dy = yy * vc->vc_font.height;
	image.height = vc->vc_font.height;
	image.depth = 1;

	if (image.dy >= vyres)
		return;

	image.height = min(image.height, vyres - image.dy);

	if (attribute) {
		buf = kmalloc(cellsize, GFP_ATOMIC);
		if (!buf)
			return;
	}

	while (count) {
		if (count > maxcnt)
			cnt = maxcnt;
		else
			cnt = count;

		image.width = vc->vc_font.width * cnt;

		if (image.dx >= info->var.xres)
			break;

		if (image.dx + image.width > info->var.xres) {
			image.width = info->var.xres - image.dx;
			cnt = image.width / vc->vc_font.width;
			if (cnt == 0)
				break;
			image.width = cnt * vc->vc_font.width;
		}

		pitch = DIV_ROUND_UP(image.width, 8) + scan_align;
		pitch &= ~scan_align;
		size = pitch * image.height + buf_align;
		size &= ~buf_align;
		dst = fb_get_buffer_offset(info, &info->pixmap, size);
		image.data = dst;

		if (!mod)
			bit_putcs_aligned(vc, info, s, attribute, cnt, pitch,
					  width, vpitch, &image, buf, dst);
		else
			bit_putcs_unaligned(vc, info, s, attribute, cnt,
					    pitch, width, vpitch, &image,
					    buf, dst);

		image.dx += cnt * vc->vc_font.width;
		count -= cnt;
		s += cnt;
	}

	/* buf is always NULL except when in monochrome mode, so in this case
	   it's a gain to check buf against NULL even though kfree() handles
	   NULL pointers just fine */
	if (unlikely(buf))
		kfree(buf);

}

static void bit_clear_margins(struct vc_data *vc, struct fb_info *info,
			      int color, int bottom_only)
{
	unsigned int cw = vc->vc_font.width;
	unsigned int ch = vc->vc_font.height;
	unsigned int rw = info->var.xres - (vc->vc_cols*cw);
	unsigned int bh = info->var.yres - (vc->vc_rows*ch);
	unsigned int rs = info->var.xres - rw;
	unsigned int bs = info->var.yres - bh;
	struct fb_fillrect region;

	region.color = color;
	region.rop = ROP_COPY;

	if ((int) rw > 0 && !bottom_only) {
		region.dx = info->var.xoffset + rs;
		region.dy = 0;
		region.width = rw;
		region.height = info->var.yres_virtual;
		info->fbops->fb_fillrect(info, &region);
	}

	if ((int) bh > 0) {
		region.dx = info->var.xoffset;
		region.dy = info->var.yoffset + bs;
		region.width = rs;
		region.height = bh;
		info->fbops->fb_fillrect(info, &region);
	}
}

static void bit_cursor(struct vc_data *vc, struct fb_info *info, bool enable,
		       int fg, int bg)
{
	struct fb_cursor cursor;
	struct fbcon_par *par = info->fbcon_par;
	unsigned short charmask = vc->vc_hi_font_mask ? 0x1ff : 0xff;
	int width = vc->vc_font.width;
	int vpitch = vc->vc_font.height;
	int cellsize = font_glyph_size(width, vpitch);
	font_data_t *fd = (font_data_t *)vc->vc_font.data; /* upcast to font_data_t */
	int c;
	int y = real_y(par->p, vc->state.y);
	int attribute, use_sw = vc->vc_cursor_type & CUR_SW;
	int err = 1;
	const unsigned char *src;

	cursor.set = 0;

	if (!vc->vc_font.data)
		return;

	c = scr_readw((u16 *) vc->vc_pos);
	attribute = get_attribute(info, c);
	c &= charmask;

#ifdef CONFIG_FONT_CJK
	{
		bool continuation;
		u32 c_utf = vc_uniscr_get_cjk(vc, vc->state.y, vc->state.x, &continuation);
		u32 cellsize = font_glyph_size(width, vpitch);

		src = font_bits(vc, c, c_utf, continuation, cellsize, NULL);
	}
#else
	src = font_data_glyph_buf(fd, width, vpitch, c);
#endif
	if (!src) {
		src = font_data_glyph_buf(fd, width, vpitch, '\0');
		if (!src)
			return;
	}

	if (par->cursor_state.image.data != (const char *)src ||
	    par->cursor_reset) {
		par->cursor_state.image.data = (const char *)src;
		cursor.set |= FB_CUR_SETIMAGE;
	}

	if (attribute) {
		unsigned char *dst;

		dst = kmalloc(cellsize, GFP_ATOMIC);
		if (!dst)
			return;
		kfree(par->cursor_data);
		par->cursor_data = dst;
		update_attr(dst, src, attribute, vc);
		src = dst;
	}

	if (par->cursor_state.image.fg_color != fg ||
	    par->cursor_state.image.bg_color != bg ||
	    par->cursor_reset) {
		par->cursor_state.image.fg_color = fg;
		par->cursor_state.image.bg_color = bg;
		cursor.set |= FB_CUR_SETCMAP;
	}

	if ((par->cursor_state.image.dx != (vc->vc_font.width * vc->state.x)) ||
	    (par->cursor_state.image.dy != (vc->vc_font.height * y)) ||
	    par->cursor_reset) {
		par->cursor_state.image.dx = vc->vc_font.width * vc->state.x;
		par->cursor_state.image.dy = vc->vc_font.height * y;
		cursor.set |= FB_CUR_SETPOS;
	}

	if (par->cursor_state.image.height != vc->vc_font.height ||
	    par->cursor_state.image.width != vc->vc_font.width ||
	    par->cursor_reset) {
		par->cursor_state.image.height = vc->vc_font.height;
		par->cursor_state.image.width = vc->vc_font.width;
		cursor.set |= FB_CUR_SETSIZE;
	}

	if (par->cursor_state.hot.x || par->cursor_state.hot.y ||
	    par->cursor_reset) {
		par->cursor_state.hot.x = cursor.hot.y = 0;
		cursor.set |= FB_CUR_SETHOT;
	}

	if (cursor.set & FB_CUR_SETSIZE ||
	    vc->vc_cursor_type != par->p->cursor_shape ||
	    par->cursor_state.mask == NULL ||
	    par->cursor_reset) {
		unsigned char *mask = fbcon_cursor_glyph(&vc->vc_font, vc->vc_cursor_type);

		if (!mask)
			return;

		kfree(par->cursor_state.mask);
		par->cursor_state.mask = (const char *)mask;

		par->p->cursor_shape = vc->vc_cursor_type;
		cursor.set |= FB_CUR_SETSHAPE;
	}

	par->cursor_state.enable = enable && !use_sw;

	cursor.image.data = src;
	cursor.image.fg_color = par->cursor_state.image.fg_color;
	cursor.image.bg_color = par->cursor_state.image.bg_color;
	cursor.image.dx = par->cursor_state.image.dx;
	cursor.image.dy = par->cursor_state.image.dy;
	cursor.image.height = par->cursor_state.image.height;
	cursor.image.width = par->cursor_state.image.width;
	cursor.hot.x = par->cursor_state.hot.x;
	cursor.hot.y = par->cursor_state.hot.y;
	cursor.mask = par->cursor_state.mask;
	cursor.enable = par->cursor_state.enable;
	cursor.image.depth = 1;
	cursor.rop = ROP_XOR;

	if (info->fbops->fb_cursor)
		err = info->fbops->fb_cursor(info, &cursor);

	if (err)
		soft_cursor(info, &cursor);

	par->cursor_reset = 0;
}

static int bit_update_start(struct fb_info *info)
{
	struct fbcon_par *par = info->fbcon_par;
	int err;

	err = fb_pan_display(info, &par->var);
	par->var.xoffset = info->var.xoffset;
	par->var.yoffset = info->var.yoffset;
	par->var.vmode = info->var.vmode;
	return err;
}

static const struct fbcon_bitops bit_fbcon_bitops = {
	.bmove = bit_bmove,
	.clear = bit_clear,
	.putcs = bit_putcs,
	.clear_margins = bit_clear_margins,
	.cursor = bit_cursor,
	.update_start = bit_update_start,
};

void fbcon_set_bitops_ur(struct fbcon_par *par)
{
	par->bitops = &bit_fbcon_bitops;
}
