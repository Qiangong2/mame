// license:BSD-3-Clause
// copyright-holders:Jarek Parchanski
/***************************************************************************

  video.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "emu.h"
#include "pingpong.h"



/***************************************************************************

  Convert the color PROMs into a more useable format.

  Ping Pong has a 32 bytes palette PROM and two 256 bytes color lookup table
  PROMs (one for sprites, one for characters).
  I don't know for sure how the palette PROM is connected to the RGB output,
  but it's probably the usual:

  bit 7 -- 220 ohm resistor  -- BLUE
        -- 470 ohm resistor  -- BLUE
        -- 220 ohm resistor  -- GREEN
        -- 470 ohm resistor  -- GREEN
        -- 1  kohm resistor  -- GREEN
        -- 220 ohm resistor  -- RED
        -- 470 ohm resistor  -- RED
  bit 0 -- 1  kohm resistor  -- RED

***************************************************************************/
void pingpong_state::pingpong_palette(palette_device &palette) const
{
	const uint8_t *color_prom = memregion("proms")->base();

	// create a lookup table for the palette
	for (int i = 0; i < 0x20; i++)
	{
		int bit0, bit1, bit2;

		// red component
		bit0 = BIT(color_prom[i], 0);
		bit1 = BIT(color_prom[i], 1);
		bit2 = BIT(color_prom[i], 2);
		int const r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		// green component
		bit0 = BIT(color_prom[i], 3);
		bit1 = BIT(color_prom[i], 4);
		bit2 = BIT(color_prom[i], 5);
		int const g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		// blue component
		bit0 = 0;
		bit1 = BIT(color_prom[i], 6);
		bit2 = BIT(color_prom[i], 7);
		int const b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		palette.set_indirect_color(i, rgb_t(r, g, b));
	}

	// color_prom now points to the beginning of the lookup table
	color_prom += 0x20;

	// characters
	for (int i = 0; i < 0x100; i++)
	{
		uint8_t const ctabentry = (color_prom[i] & 0x0f) | 0x10;
		palette.set_pen_indirect(i, ctabentry);
	}

	// sprites
	for (int i = 0x100; i < 0x200; i++)
	{
		uint8_t const ctabentry = bitswap<8>(color_prom[i],7,6,5,4,0,1,2,3);
		palette.set_pen_indirect(i, ctabentry);
	}
}

void pingpong_state::pingpong_videoram_w(offs_t offset, uint8_t data)
{
	m_videoram[offset] = data;
	m_bg_tilemap->mark_tile_dirty(offset);
}

void pingpong_state::pingpong_colorram_w(offs_t offset, uint8_t data)
{
	m_colorram[offset] = data;
	m_bg_tilemap->mark_tile_dirty(offset);
}

TILE_GET_INFO_MEMBER(pingpong_state::get_bg_tile_info)
{
	int attr = m_colorram[tile_index];
	int code = m_videoram[tile_index] + ((attr & 0x20) << 3);
	int color = attr & 0x1f;
	int flags = ((attr & 0x40) ? TILE_FLIPX : 0) | ((attr & 0x80) ? TILE_FLIPY : 0);

	tileinfo.set(0, code, color, flags);
}

void pingpong_state::video_start()
{
	m_bg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(pingpong_state::get_bg_tile_info)), TILEMAP_SCAN_ROWS, 8, 8, 32, 32);
}

void pingpong_state::draw_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	/* This is strange; it's unlikely that the sprites actually have a hardware */
	/* clipping region, but I haven't found another way to have them masked by */
	/* the characters at the top and bottom of the screen. */
	const rectangle spritevisiblearea(0*8, 32*8-1, 4*8, 29*8-1);

	uint8_t *spriteram = m_spriteram;
	int offs;

	for (offs = m_spriteram.bytes() - 4;offs >= 0;offs -= 4)
	{
		int sx,sy,flipx,flipy,color,schar;


		sx = spriteram[offs + 3];
		sy = 241 - spriteram[offs + 1];

		flipx = spriteram[offs] & 0x40;
		flipy = spriteram[offs] & 0x80;
		color = spriteram[offs] & 0x1f;
		schar = spriteram[offs + 2] & 0x7f;

		m_gfxdecode->gfx(1)->transmask(bitmap,spritevisiblearea,
				schar,
				color,
				flipx,flipy,
				sx,sy,
				m_palette->transpen_mask(*m_gfxdecode->gfx(1), color, 0));
	}
}

uint32_t pingpong_state::screen_update_pingpong(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_bg_tilemap->draw(screen, bitmap, cliprect, 0, 0);
	draw_sprites(bitmap, cliprect);
	return 0;
}
