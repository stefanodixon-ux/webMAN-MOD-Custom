#include "include/blitting.h"
#include "include/png_dec.h"
#include "include/misc.h"
#include "include/mem.h"

#include <cell/rtc.h>
#include <cell/cell_fs.h>
//#include "include/network.h"	// debug


// display values
static uint32_t unk1 = 0, offset = 0, pitch = 0;
static uint32_t h = 0, w = 0, canvas_x = 0, canvas_y = 0;

static Bitmap *bitmap = NULL;                       // font glyph cache

static const CellFontLibrary* font_lib_ptr = NULL;  // font library pointer
static uint32_t vsh_fonts[16] = {};                 // addresses of the 16 system font slots

int32_t LINE_HEIGHT = 0;

/***********************************************************************
* get font object
***********************************************************************/
static int32_t get_font_object(void)
{
	int32_t i, font_obj = 0;
	int32_t pm_start = 0x10000;
	uint64_t pat[2] = {0x3800001090810080ULL, 0x90A100849161008CULL};

	while(pm_start < 0x700000)
	{
		if((*(uint64_t*)pm_start == pat[0]) && (*(uint64_t*)(pm_start+8) == pat[1]))
		{
			// get font object
			font_obj = (int32_t)((int32_t)((*(int32_t*)(pm_start + 0x4C) & 0x0000FFFF) <<16) +
					   (int16_t)( *(int32_t*)(pm_start + 0x54) & 0x0000FFFF));

			// get font library pointer
			font_lib_ptr = (void*)(*(int32_t*)font_obj);

			// get addresses of loaded sys fonts
			for(i = 0; i < 16; i++)
				vsh_fonts[i] = (font_obj + 0x14 + (i * 0x100));

			return 0;
		}

		pm_start += 4;
	}

	return FAILED;
}

/***********************************************************************
* set font with default settings
***********************************************************************/
static void set_font_default(void)
{
	int32_t i;
	bitmap = mem_alloc(sizeof(Bitmap));
	memset(bitmap, 0, sizeof(Bitmap));

	// set font
	FontSetScalePixel(&ctx.font, FONT_W, FONT_H);
	FontSetEffectWeight(&ctx.font, FONT_WEIGHT);

	FontGetHorizontalLayout(&ctx.font, &bitmap->horizontal_layout);
	LINE_HEIGHT = bitmap->horizontal_layout.lineHeight;

	bitmap->max    = FONT_CACHE_MAX;
	bitmap->count  = 0;
	bitmap->font_w = FONT_W;
	bitmap->font_h = FONT_H;
	bitmap->weight = FONT_WEIGHT;

	for(i = 0; i < FONT_CACHE_MAX; i++)
	  bitmap->glyph[i].image = (uint8_t *)ctx.font_cache + (i * 0x400);
}

static int get_theme_font(int user_id)
{
	int reg = -1;
	int reg_value = -1;
	uint16_t off_string, len_data, len_string;
	uint64_t r;
	char string[256];

	if(cellFsOpen("/dev_flash2/etc/xRegistry.sys", CELL_FS_O_RDONLY, &reg, NULL, 0) != CELL_FS_SUCCEEDED || reg == -1)
	{
		return reg_value;
	}

	CellFsStat stat;
	cellFsStat("/dev_flash2/etc/xRegistry.sys", &stat);
	uint64_t entry_offset = 0x10000;

	char key[40]; sprintf(key, "/setting/user/%08i/theme/font", user_id);

	for(;;)
	{
	//// Data entries ////
		//unk
		entry_offset += 2;

		//relative entry offset
		cellFsReadWithOffset(reg, entry_offset, &off_string, 2, &r);
		entry_offset += 4;

		//data lenght
		cellFsReadWithOffset(reg, entry_offset, &len_data, 2, &r);
		entry_offset += 3;

	//// String Entries ////
		off_string += 0x12;

		//string length
		cellFsReadWithOffset(reg, off_string, &len_string, 2, &r);
		off_string += 3;

		//string
		memset(string, 0, sizeof(string));
		cellFsReadWithOffset(reg, off_string, string, len_string, &r);

		//Find key
		if(!strcmp(string, key))
		{
			if(len_data == 4)
				cellFsReadWithOffset(reg, entry_offset, &reg_value, 4, &r);
			break;
		}

		entry_offset += len_data + 1;

		if(off_string == 0xCCDD || entry_offset >= stat.st_size) break;
	}

	cellFsClose(reg);

	return reg_value;
}

/***********************************************************************
* unbind and destroy renderer, close font instance
***********************************************************************/
static void font_init(void)
{
	uint32_t user_id = 0; int sysfont = 0;
	CellFontRendererConfig rd_cfg;
	CellFont *opened_font = NULL;

	if(get_font_object() == FAILED) return;

	// get id of current logged in user for the xRegistry query we do next
	user_id = xsetting_CC56EB2D()->GetCurrentUserNumber();
	if(user_id > 255) user_id = 1;

	// get current font style for the current logged in user
	xsetting_CC56EB2D()->GetRegistryValue(user_id, 0x5C, &sysfont);

	// fix font selection
	int lang_id = 1;
	xsetting_0AF1F161()->GetSystemLanguage(&lang_id);
	if(((lang_id >= 9) && (lang_id <= 11)) || (lang_id == 16) || (lang_id == 19)) //kor / chi-tra / chi-sim / pol
		sysfont = 0;
	if(lang_id == 7) // rus
		sysfont = 4;
	if(lang_id == 8) // jap
		sysfont = get_theme_font(user_id);

	// get sysfont
	switch(sysfont)
	{
		case 0:   // original
		  opened_font = (void*)(vsh_fonts[5]);
		  break;
		case 1:   // rounded
		  opened_font = (void*)(vsh_fonts[8]);
		  break;
		case 3:   // pop
		  opened_font = (void*)(vsh_fonts[10]);
		  break;
		case 4:   // russian
		  opened_font = (void*)(vsh_fonts[1]);
		  break;
		default:  // better than nothing
		  opened_font = (void*)(vsh_fonts[9]);
		  break;
	}

	if(!opened_font) return;

	FontOpenFontInstance(opened_font, &ctx.font);

	memset(&rd_cfg, 0, sizeof(CellFontRendererConfig));
	FontCreateRenderer(font_lib_ptr, &rd_cfg, &ctx.renderer);

	FontBindRenderer(&ctx.font, &ctx.renderer);

	set_font_default();
}

/***********************************************************************
* unbind and destroy renderer, close font instance
***********************************************************************/
void font_finalize(void)
{
  FontUnbindRenderer(&ctx.font);
  FontDestroyRenderer(&ctx.renderer);
  FontCloseFont(&ctx.font);
}

/***********************************************************************
* render a char glyph bitmap into bitmap cache by index
*
* int32_t cache_idx  =  index into cache
* uint32_t code      =  unicode of char glyph to render
***********************************************************************/
static void render_glyph(int32_t idx, uint32_t code)
{
	CellFontRenderSurface  surface;
	CellFontGlyphMetrics   metrics;
	CellFontImageTransInfo transinfo;
	int32_t i, k, m, x, y, w, h;
	int32_t ibw;


	// setup render settings
	FontSetupRenderScalePixel(&ctx.font, bitmap->font_w, bitmap->font_h);
	FontSetupRenderEffectWeight(&ctx.font, bitmap->weight);

	x = ((int32_t)bitmap->font_w) * 2;
	y = ((int32_t)bitmap->font_h) * 2;
	w = x * 2;
	h = y * 2;

	// set surface
	FontRenderSurfaceInit(&surface, NULL, w, 1, w, h);

	// set render surface scissor, (full area/no scissoring)
	FontRenderSurfaceSetScissor(&surface, 0, 0, w, h);

	bitmap->glyph[idx].code = code;

	FontRenderCharGlyphImage(&ctx.font, bitmap->glyph[idx].code, &surface,
							(float_t)x, (float_t)y, &metrics, &transinfo);

	bitmap->count++;

	ibw = transinfo.imageWidthByte;
	bitmap->glyph[idx].w = transinfo.imageWidth;      // width of char image
	bitmap->glyph[idx].h = transinfo.imageHeight;     // height of char image

	// copy glyph bitmap into cache
	for(k = 0; k < bitmap->glyph[idx].h; k++)
		for(m = k*bitmap->glyph[idx].w, i = 0; i < bitmap->glyph[idx].w; i++)
			bitmap->glyph[idx].image[m + i] =
			transinfo.Image[k * ibw + i];

	bitmap->glyph[idx].metrics = metrics;
}

/***********************************************************************
*
***********************************************************************/
static Glyph *get_glyph(uint32_t code)
{
	int32_t i, new;
	Glyph *glyph;


	// search glyph into cache
	for(i = 0; i < bitmap->count; i++)
	{
		glyph = &bitmap->glyph[i];

		if(glyph->code == code)
			return glyph;
	}

	// if glyph not into cache
	new = bitmap->count + 1;

	if(new >= bitmap->max)       // if cache full
	  bitmap->count = new = 0;   // reset

	// render glyph
	render_glyph(new, code);
	glyph = &bitmap->glyph[new];

	return glyph;
}

/***********************************************************************
* get ucs4 code from utf8 sequence
*
* uint8_t *utf8   =  utf8 string
* uint32_t *ucs4  =  variable to hold ucs4 code
***********************************************************************/
static int32_t utf8_to_ucs4(uint8_t *utf8, uint32_t *ucs4)
{
	int32_t len = 0;
	uint32_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;


	c1 = (uint32_t)*utf8;
	utf8++;

	if(c1 <= 0x7F)                        // 1 byte sequence, ascii
	{
		len = 1;
		*ucs4 = c1;
	}
	else if((c1 & 0xE0) == 0xC0)          // 2 byte sequence
	{
		len = 2;
		c2 = (uint32_t)*utf8;

		if((c2 & 0xC0) == 0x80)
			*ucs4 = ((c1  & 0x1F) << 6) | (c2 & 0x3F);
		else
			len = *ucs4 = 0;
	}
	else if((c1 & 0xF0) == 0xE0)          // 3 bytes sequence
	{
		len = 3;
		c2 = (uint32_t)*utf8;
		utf8++;

		if((c2 & 0xC0) == 0x80)
		{
			c3 = (uint32_t)*utf8;

			if((c3 & 0xC0) == 0x80)
				*ucs4 = ((c1  & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
			else
				len = *ucs4 = 0;
		}
		else
			len = *ucs4 = 0;
	}
	else if((c1 & 0xF8) == 0xF0)          // 4 bytes sequence
	{
		len = 4;
		c2 = (uint32_t)*utf8;
		utf8++;

		if((c2 & 0xC0) == 0x80)
		{
			c3 = (uint32_t)*utf8;
			utf8++;

			if((c3 & 0xC0) == 0x80)
			{
				c4 = (uint32_t)*utf8;

				if((c4 & 0xC0) == 0x80)
				*ucs4 = ((c1  & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) <<  6) | (c4 & 0x3F);
				else
				  len = *ucs4 = 0;
			}
			else
			  len = *ucs4 = 0;
		}
		else
		  len = *ucs4 = 0;
	}
	else
		len = *ucs4 = 0;

	return len;
}

/***********************************************************************
* dump background
***********************************************************************/
static void dump_bg(void)
{
	uint32_t i, k, kk, m, y, CANVAS_WW = CANVAS_W/2;
	uint64_t *bg = (uint64_t*)ctx.bg;

	for(i = 0; i < CANVAS_H; i++)
		for(m = i * CANVAS_WW,
			kk = canvas_x, y = canvas_y + i,
			k = 0; k < CANVAS_WW; k++, kk+=2)
				bg[k + m] = *(uint64_t*)(OFFSET(kk, y));
}

/***********************************************************************
*
***********************************************************************/
void init_graphic()
{
	memset(&ctx, 0, sizeof(DrawCtx));

	// set drawing context
	ctx.canvas     = mem_alloc(CANVAS_W * CANVAS_H * 4);  // canvas buffer
	ctx.bg         = mem_alloc(CANVAS_W * CANVAS_H * 4);  // background buffer
	ctx.font_cache = mem_alloc(FONT_CACHE_MAX * 32 * 32); // glyph bitmap cache
	ctx.bg_color   = 0xFF000000;                          // black, opaque
	ctx.fg_color   = 0xFFFFFFFF;                          // white, opaque

	font_init();

	// get current display values
	offset = BASE + *(uint32_t*)0x60201104;      // start offset of current framebuffer
	getDisplayPitch(&pitch, &unk1); pitch /= 4;  // framebuffer pitch size
	h = getDisplayHeight();                      // display height
	w = getDisplayWidth();                       // display width

	// get x/y start coordinates for our canvas, here always center
	canvas_x = (w - CANVAS_W) / 2;
	canvas_y = (h - CANVAS_H) / 2;

	// dump background, for alpha blending
	dump_bg();

	// init first frame with background dump
	memcpy32(ctx.canvas, ctx.bg, CANVAS_W * CANVAS_H);
}

/***********************************************************************
* load a png file
*
* int32_t idx      = index of png, max 4 (0 - 3)
* const char *path = path to png file
***********************************************************************/
int32_t load_png_bitmap(int32_t idx, const char *path)
{
	if(idx > PNG_MAX) return FAILED;

	mem_free(ctx.png[idx].w * ctx.png[idx].h * 4);

	ctx.png[idx] = load_png(path);

	return 0;
}

/***********************************************************************
* alpha blending (ARGB)
*
* uint32_t bg = background color
* uint32_t fg = foreground color
***********************************************************************/
static uint32_t mix_color(uint32_t bg, uint32_t fg)
{
	uint32_t a = fg >>24;

	if(a == 0) return bg;

	uint32_t aa = (255 - a);

	uint32_t rb = (((fg & 0x00FF00FF) * a) + ((bg & 0x00FF00FF) * aa)) & 0xFF00FF00;
	uint32_t g  = (((fg & 0x0000FF00) * a) + ((bg & 0x0000FF00) * aa)) & 0x00FF0000;
	fg = a + (((bg >>24) * aa)>>8);

	return (fg <<24) | ((rb | g) >>8);
}

/***********************************************************************
* flip finished frame into paused ps3-framebuffer
***********************************************************************/
void flip_frame()
{
	int32_t i, k, kk, m, y, CANVAS_WW = CANVAS_W/2;
	uint64_t *canvas = (uint64_t*)ctx.canvas;

	for(i = 0; i < CANVAS_H; i++)
		for(m = i * CANVAS_WW,
			kk = canvas_x, y = canvas_y + i,
			k = 0; k < CANVAS_WW; k++, kk+=2)
				*(uint64_t*)(OFFSET(kk, y)) = canvas[k + m];

	// after flip, clear frame buffer with background
	memcpy32(ctx.canvas, ctx.bg, CANVAS_W * CANVAS_H);
}

/***********************************************************************
* set background color
***********************************************************************/
void set_background_color(uint32_t color)
{
	ctx.bg_color = color;
}

/***********************************************************************
* set foreground color
***********************************************************************/
void set_foreground_color(uint32_t color)
{
	ctx.fg_color = color;
}

/***********************************************************************
* set new font values
*
* float_t font_w    =  char width
* float_t font_h    =  char height
* float_t weight    =  line weight
* int32_t distance  =  distance between chars
***********************************************************************/
void set_font(float_t font_w, float_t font_h, float_t weight, int32_t distance)
{
	// max size is 32 * 32 pixels
	if(font_w > 32.f) font_w = 32.f;
	if(font_h > 32.f) font_h = 32.f;

	// set font
	FontSetScalePixel(&ctx.font, font_w, font_h);
	FontSetEffectWeight(&ctx.font, weight);

	// get and set new line height
	FontGetHorizontalLayout(&ctx.font, &bitmap->horizontal_layout);
	LINE_HEIGHT = bitmap->horizontal_layout.lineHeight;

	bitmap->count    = 0;                             // reset font cache
	bitmap->font_w   = font_w;
	bitmap->font_h   = font_h;
	bitmap->weight   = weight;
	bitmap->distance = distance;
}

/***********************************************************************
* draw background, with current background color
***********************************************************************/
void draw_background()
{
	const uint32_t CANVAS_SIZE = CANVAS_W * CANVAS_H;

	for(uint32_t i = 0; i < CANVAS_SIZE; i++)
		ctx.canvas[i] = mix_color(ctx.bg[i], ctx.bg_color);
}

/***********************************************************************
* print text, (TTF)
*
* int32_t x       = start x coordinate into canvas
* int32_t y       = start y coordinate into canvas
* const char *str = string to print
***********************************************************************/
int32_t print_text(int32_t x, int32_t y, const char *str)
{
	int32_t i, k, len = 0;
	uint32_t code = 0;                                              // char unicode
	int32_t t_x = x, t_y = y;                                       // temp x/y
	int32_t o_x = x, o_y = y + bitmap->horizontal_layout.baseLineY; // origin x/y
	Glyph *glyph;                                                   // char glyph
	uint8_t *utf8 = (uint8_t*)str; if(!str) return x;
	uint32_t pixel;

	memset(&glyph, 0, sizeof(Glyph));

	// center text (only 1 line)
	if(x == CENTER_TEXT)
	{
		for(;;) // get render length
		{
			utf8 += utf8_to_ucs4(utf8, &code);

			if(code == 0) break;

			glyph = get_glyph(code);
			len += glyph->metrics.Horizontal.advance + bitmap->distance;
		}

		o_x = t_x = (CANVAS_W - len - bitmap->distance) / 2;
		utf8 = (uint8_t*)str;
	}

	// render text
	for(;;)
	{
		utf8 += utf8_to_ucs4(utf8, &code);

		if(code == 0) break;

		if((code == '^') || ((code == '\n') && (x != CENTER_TEXT)))
		{
			o_x = x;
			o_y += bitmap->horizontal_layout.lineHeight;
			continue;
		}
		else
		{
			// get glyph to draw
			glyph = get_glyph(code);

			// get bitmap origin(x, y)
			t_x = o_x + glyph->metrics.Horizontal.bearingX;
			t_y = o_y - glyph->metrics.Horizontal.bearingY;

			// draw bitmap
			for(i = 0; i < glyph->h; i++)
			  if(t_y + i < CANVAS_H)
				  for(k = 0; k < glyph->w; k++)
					if((glyph->image[i * glyph->w + k]) && (t_x + k < CANVAS_W))
					{
						pixel = (t_y + i) * CANVAS_W + t_x + k;
						ctx.canvas[pixel] =
						mix_color(ctx.canvas[pixel],
								 ((uint32_t)glyph->image[i * glyph->w + k] <<24) |
								 (ctx.fg_color & 0x00FFFFFF));
					}

			// get origin-x for next char
			o_x += glyph->metrics.Horizontal.advance + bitmap->distance;
		}
	}

	return o_x;
}

/***********************************************************************
* draw png part into frame.
*
* int32_t can_x	=  start x coordinate into canvas
* int32_t can_y	=  start y coordinate into canvas
* int32_t png_x	=  start x coordinate into png
* int32_t png_y	=  start y coordinate into png
* int32_t w		=  width of png part to blit
* int32_t h		=  height of png part to blit
***********************************************************************/
int32_t draw_png(int32_t idx, int32_t c_x, int32_t c_y, int32_t p_x, int32_t p_y, int32_t w, int32_t h)
{
	uint32_t i, k, m, o, hh = h, ww = w;

	const uint32_t CANVAS_WW = CANVAS_W - c_x, CANVAS_HH = CANVAS_H - c_y;

	if(ww > CANVAS_WW) ww = CANVAS_WW;
	if(hh > CANVAS_HH) hh = CANVAS_HH;

	uint32_t offset = p_x + p_y * ctx.png[idx].w;

	for(i = 0; i < hh; i++)
		for(m = (c_y + i) * CANVAS_W + c_x,
			o = offset + i * ctx.png[idx].w,
			k = 0; k < ww; k++, m++, o++)
				ctx.canvas[m] =
					mix_color(ctx.canvas[m],
					ctx.png[idx].addr[o]);

	return (c_x + w);
}

// screenshot
uint8_t bmp_header[] = {
  0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/***********************************************************************
* xmb screenshot
*
* uint8_t mode = 0(XMB only), 1(XMB + menu)
***********************************************************************/
void screenshot(uint8_t mode)
{
	char path[64];

	// build file path
	CellRtcDateTime t;
	cellRtcGetCurrentClockLocalTime(&t);
	sprintf(path, "/dev_hdd0/screenshot_%02d_%02d_%02d_%02d_%02d_%02d.bmp", t.year, t.month, t.day, t.hour, t.minute, t.second);

	// create bmp file
	FILE *fd = NULL;
	fd = fopen(path, "wb"); if(!fd) return;

	// alloc buffers
	sys_memory_container_t mc_app = (sys_memory_container_t)-1;
	mc_app = vsh_memory_container_by_id(1);

	const int32_t mem_size = 64 * 1024; // 64 KB (bmp data and frame buffer)

	// max frame line size = 1920 pixel * 4(byte per pixel) = 7680 byte = 8 KB
	// max bmp buffer size = 1920 pixel * 3(byte per pixel) = 5760 byte = 6 KB

	sys_addr_t sys_mem = NULL;
	sys_memory_allocate_from_container(mem_size, mc_app, SYS_MEMORY_PAGE_SIZE_64K, &sys_mem);

	// calc buffer sizes
	uint32_t line_frame_size = w * 4;

	// alloc buffers
	uint64_t *line_frame = (uint64_t*)sys_mem;
	uint8_t *bmp_buf = (uint8_t*)sys_mem + (4 * line_frame_size); // start offset: 30 KB
	uint64_t *bg = (uint64_t*)ctx.bg;

	// set bmp header
	uint32_t tmp = 0;
	tmp = _ES32(w*h*3+0x36);
	memcpy32(bmp_header + 2 , &tmp, 1);     // file size
	tmp = _ES32(w);
	memcpy32(bmp_header + 18, &tmp, 1);     // bmp width
	tmp = _ES32(h);
	memcpy32(bmp_header + 22, &tmp, 1);     // bmp height
	tmp = _ES32(w*h*3);
	memcpy32(bmp_header + 34, &tmp, 1);     // bmp data size

	// write bmp header
	fwrite(bmp_header, 1, sizeof(bmp_header), fd);

	uint32_t c, i, k, kk, m, idx = 0;

	uint32_t canvas_x_left = canvas_x/2, canvas_x_right = canvas_x_left + CANVAS_W/2, canvas_y_bottom = canvas_y + CANVAS_H, ww=w/2;

	// dump...
	for(c = 0, i = h; i > 0; i--, c++)
	{
		for(k = kk = 0; k < ww; k++, kk+=2)
			line_frame[k] = *(uint64_t*)(OFFSET(kk, i));

		if((mode == 0) && (i >= canvas_y) && (i < canvas_y_bottom))
		{
			m = (((i - canvas_y) * CANVAS_W) - canvas_x)>>1;
			for(k = canvas_x_left; k < canvas_x_right; k++)
			{
				line_frame[k] = bg[m + k];
			}
		}

		// convert line from ARGB to RGB
		uint8_t *tmp_buf = (uint8_t*)line_frame;

		for(k = 0; k < line_frame_size; k+=4, idx+=3)
		{
			bmp_buf[idx]   = tmp_buf[k + 3];  // R
			bmp_buf[idx+1] = tmp_buf[k + 2];  // G
			bmp_buf[idx+2] = tmp_buf[k + 1];  // B
		}

		// write bmp data
		if(c >= 3)
		{
			fwrite(bmp_buf, 1, idx, fd);
			c = idx = 0;
		}
	}
	fwrite(bmp_buf, 1, idx, fd);

	// padding
	int32_t rest = (w*3) % 4, pad = 0;
	if(rest)
		pad = 4 - rest;
	fseek(fd, pad, SEEK_CUR);

	fclose(fd);
	sys_memory_free((sys_addr_t)sys_mem);

	vshtask_notify(path);
}




// some primitives...
/***********************************************************************
* draw a single pixel,
*
* int32_t x  =  start x coordinate into frame
* int32_t y  =  start y coordinate into frame
**********************************************************************
void draw_pixel(int32_t x, int32_t y)
{
  if((x < CANVAS_W) && (y < CANVAS_H))
	  ctx.canvas[x + y * CANVAS_W] = ctx.fg_color;
}*/

/***********************************************************************
* draw a line,
*
* int32_t x   =  line start x coordinate into frame
* int32_t y   =  line start y coordinate into frame
* int32_t x2  =  line end x coordinate into frame
* int32_t y2  =  line end y coordinate into frame
**********************************************************************
void draw_line(int32_t x, int32_t y, int32_t x2, int32_t y2)
{
	int32_t i = 0, dx1 = 0, dy1 = 0, dx2 = 0, dy2 = 0;
	int32_t w = x2 - x;
	int32_t h = y2 - y;


	if(w < 0) dx1 = -1; else if(w > 0) dx1 = 1;
	if(h < 0) dy1 = -1; else if(h > 0) dy1 = 1;
	if(w < 0) dx2 = -1; else if(w > 0) dx2 = 1;

	int32_t l = abs(w);
	int32_t s = abs(h);

	if(!(l > s))
	{
		l = abs(h);
		s = abs(w);

		if(h < 0) dy2 = -1; else if(h > 0) dy2 = 1;

		dx2 = 0;
	}

	int32_t num = l >> 1;

	for(i = 0; i <= l; i++)
	{
		draw_pixel(x, y);
		num+=s;

		if(!(num < l))
		{
			num-=l;
			x+=dx1;
			y+=dy1;
		}
		else
		{
			x+=dx2;
			y+=dy2;
		}
	}
}*/

/***********************************************************************
* draw a rectangle,
*
* int32_t x  =  rectangle start x coordinate into frame
* int32_t y  =  rectangle start y coordinate into frame
* int32_t w  =  width of rectangle
* int32_t h  =  height of rectangle
**********************************************************************
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	draw_line(x, y, x + w, y);
	draw_line(x + w, y, x + w, y + h);
	draw_line(x + w, y + h, x, y + h);
	draw_line(x, y + h, x, y);
}*/

/***********************************************************************
* circle helper function
*
* int32_t x_c  =  circle center x coordinate into frame
* int32_t y_c  =  circle center y coordinate into frame
* int32_t x    =  circle point x coordinate into frame
* int32_t y    =  circle point y coordinate into frame
**********************************************************************
static void circle_points(int32_t x_c, int32_t y_c, int32_t x, int32_t y)
{
	draw_pixel(x_c + x, y_c + y);
	draw_pixel(x_c - x, y_c + y);
	draw_pixel(x_c + x, y_c - y);
	draw_pixel(x_c - x, y_c - y);
	draw_pixel(x_c + y, y_c + x);
	draw_pixel(x_c - y, y_c + x);
	draw_pixel(x_c + y, y_c - x);
	draw_pixel(x_c - y, y_c - x);
}*/

/***********************************************************************
* draw a circle,
*
* int32_t x_c  =  circle center x coordinate into frame
* int32_t y_c  =  circle center y coordinate into frame
* int32_t r    =  circle radius
**********************************************************************
void draw_circle(int32_t x_c, int32_t y_c, int32_t r)
{
	int32_t x = 0;
	int32_t y = r;
	int32_t p = 1 - r;

	circle_points(x_c, y_c, x, y);

	while(x < y)
	{
		x++;

		if(p < 0)
		{
			p += 2 * x + 1;
		}
		else
		{
			y--;
			p += 2 * (x - y) + 1;
		}

		circle_points(x_c, y_c, x, y);
	}
}*/
