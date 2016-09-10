#include "Renderer.hh"

namespace openmsx {

/*
Sprite palette in Graphic 7 mode.
See page 98 of the V9938 data book.
*/
const uint16_t Renderer::GRAPHIC7_SPRITE_PALETTE[16] = {
	0x000, 0x002, 0x030, 0x032, 0x300, 0x302, 0x330, 0x332,
	0x472, 0x007, 0x070, 0x077, 0x700, 0x707, 0x770, 0x777
};

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

} // namespace openmsx
