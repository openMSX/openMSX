// $Id$

/*
TODO:
- Speed up checkSpritesN by administrating which lines contain which
  sprites in a bit vector.
  This avoids cycling through all 32 possible sprites on every line.
  Keeping administration up-to-date is not that hard and happens
  at a low frequency (typically once per frame).
- Verify model for 5th sprite number calculation.
  For example, does it have the right value in text mode?
*/

#include "SpriteChecker.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPSettings.hh"
#include <cassert>

namespace openmsx {

SpriteChecker::SpriteChecker(VDP *vdp)
{
	this->vdp = vdp;
	limitSpritesSetting = VDPSettings::instance()->getLimitSprites();
	vram = vdp->getVRAM();
	vram->spriteAttribTable.setObserver(this);
	vram->spritePatternTable.setObserver(this);
}

void SpriteChecker::reset(const EmuTime &time)
{
	status = 0;
	collisionX = 0;
	collisionY = 0;

	initFrame(time);

	updateSpritesMethod = &SpriteChecker::updateSprites1;
	mode0 = false;
}

inline SpriteChecker::SpritePattern SpriteChecker::doublePattern(SpriteChecker::SpritePattern a)
{
	// bit-pattern "abcd...." gets expanded to "aabbccdd"
	a =   a                     | (a >> 16);
	a = ((a << 8) & 0x00ffff00) | (a & 0xff0000ff);
	a = ((a << 4) & 0x0ff00ff0) | (a & 0xf00ff00f);
	a = ((a << 2) & 0x3c3c3c3c) | (a & 0xc3c3c3c3);
	a = ((a << 1) & 0x66666666) | (a & 0x99999999);
	return a;
}

inline SpriteChecker::SpritePattern SpriteChecker::calculatePattern(
	int patternNr, int y)
{
	// Note: For sprite pattern, mask and index never overlap.
	const byte *patternPtr = vram->spritePatternTable.readArea(
		planar ? 0x0FC00 : 0x1F800 );
	int index = patternNr * 8 + y;
	if (planar) index = ((index << 16) | (index >> 1)) & 0x1FFFF;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp->getSpriteSize() == 16) {
		pattern |= patternPtr[index + (planar ? 8 : 16)] << 16;
	}
	return vdp->getSpriteMag() == 1 ? pattern : doublePattern(pattern);
}

// TODO: Integrate with calculatePattern.
inline SpriteChecker::SpritePattern SpriteChecker::calculatePatternNP(
	int patternNr, int y)
{
	// Note: For sprite pattern, mask and index never overlap.
	const byte *patternPtr = vram->spritePatternTable.readArea(-1 << 11);
	int index = patternNr * 8 + y;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp->getSpriteSize() == 16) {
		pattern |= patternPtr[index + 16] << 16;
	}
	return vdp->getSpriteMag() == 1 ? pattern : doublePattern(pattern);
}

inline int SpriteChecker::checkSprites1(
	int line, SpriteChecker::SpriteInfo *visibleSprites)
{
	if (!vdp->spritesEnabled()) return 0;

	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	line = line - vdp->getLineZero() + vdp->getVerticalScroll();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting->getValue();
	int sprite, visibleIndex = 0;
	int size = vdp->getSpriteSize();
	int mag = vdp->getSpriteMag();
	const byte *attributePtr = vram->spriteAttribTable.readArea(-1 << 7);
	byte patternIndexMask = size == 16 ? 0xFC : 0xFF;
	for (sprite = 0; sprite < 32; sprite++, attributePtr += 4) {
		int y = *attributePtr;
		if (y == 208) break;
		// Calculate line number within the sprite.
		int spriteLine = ((line - y) & 0xFF) / mag;
		if (spriteLine < size) {
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				if (~status & 0xC0) {
					status = (status & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex = attributePtr[2] & patternIndexMask;
			sip->pattern = calculatePatternNP(patternIndex, spriteLine);
			sip->x = attributePtr[1];
			if (attributePtr[3] & 0x80) sip->x -= 32;
			sip->colourAttrib = attributePtr[3];
		}
	}
	if (~status & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		status = (status & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (status & 0x20) return visibleIndex;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Colour doesn't matter: sprites of colour 0 can collide.
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow,
	but there are max 4 sprites and therefore max 6 pairs.
	If any collision is found, method returns at once.
	*/
	int magSize = size * mag;
	for (int i = (visibleIndex < 4 ? visibleIndex : 4); --i >= 1; ) {
		int x_i = visibleSprites[i].x;
		SpritePattern pattern_i = visibleSprites[i].pattern;
		for (int j = i; --j >= 0; ) {
			// Do sprite i and sprite j collide?
			int x_j = visibleSprites[j].x;
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				SpritePattern pattern_j = visibleSprites[j].pattern;
				if (dist < 0) {
					pattern_j <<= -dist;
				} else {
					pattern_j >>= dist;
				}
				if (pattern_i & pattern_j) {
					// Collision!
					status |= 0x20;
					// TODO: Fill in collision coordinates in S#3..S#6.
					// ...Unless this feature only works in sprite mode 2.
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

// TODO: For higher performance, have separate routines for planar and
//       non-planar modes.
inline int SpriteChecker::checkSprites2(
	int line, SpriteChecker::SpriteInfo *visibleSprites)
{
	if (!vdp->spritesEnabled()) return 0;

	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	line = line - vdp->getLineZero() + vdp->getVerticalScroll();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting->getValue();
	int sprite, visibleIndex = 0;
	int size = vdp->getSpriteSize();
	int mag = vdp->getSpriteMag();
	// TODO: Should masks be applied while processing the tables?
	//       For attribute, no (7 bits index), for
	// Bit9 is set for attribute table, reset for colour table.
	const byte *attributePtr = vram->spriteAttribTable.readArea(
		planar ? 0x0FF00 : 0x1FE00 );
	byte patternIndexMask = size == 16 ? 0xFC : 0xFF;
	int index1 = planar ? 0x10000 : 1;
	int index2 = planar ? 0x00001 : 2;
	int indexIncr = planar ? 2 : 4;
	// TODO: Verify CC implementation.
	for (sprite = 0; sprite < 32; sprite++, attributePtr += indexIncr) {
		int y = attributePtr[0];
		if (y == 216) break;
		// Calculate line number within the sprite.
		int spriteLine = ((line - y) & 0xFF) / mag;
		if (spriteLine < size) {
			if (visibleIndex == 8) {
				// Nine sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero. Stuck to this for V9938.
				if (~status & 0xC0) {
					status = (status & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			int colourIndex = (-1 << 10) | (sprite * 16 + spriteLine);
			if (planar) colourIndex =
				((colourIndex << 16) | (colourIndex >> 1)) & 0x1FFFF;
			byte colourAttrib = vram->spriteAttribTable.readNP(colourIndex);
			// Sprites with CC=1 are only visible if preceded by
			// a sprite with CC=0.
			if ((colourAttrib & 0x40) && visibleIndex == 0) continue;
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex = attributePtr[index2] & patternIndexMask;
			sip->pattern = calculatePattern(patternIndex, spriteLine);
			sip->x = attributePtr[index1];
			if (colourAttrib & 0x80) sip->x -= 32;
			sip->colourAttrib = colourAttrib;
		}
	}
	if (~status & 0x40) {
		// No 9th sprite detected, store number of latest sprite processed.
		status = (status & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (status & 0x20) return visibleIndex;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Colour doesn't matter: sprites of colour 0 can collide.
	  TODO: V9938 data book denies this (page 98).
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow.
	There are max 8 sprites and therefore max 42 pairs.
	  TODO: Maybe this is slow... Think of something faster.
	        Probably new approach is needed anyway for OR-ing.
	If any collision is found, method returns at once.
	*/
	int magSize = size * mag;
	for (int i = (visibleIndex < 8 ? visibleIndex : 8); --i >= 1; ) {
		// If CC or IC is set, this sprite cannot collide.
		if (visibleSprites[i].colourAttrib & 0x60) continue;

		int x_i = visibleSprites[i].x;
		SpritePattern pattern_i = visibleSprites[i].pattern;
		for (int j = i; --j >= 0; ) {
			// If CC or IC is set, this sprite cannot collide.
			if (visibleSprites[j].colourAttrib & 0x60) continue;

			// Do sprite i and sprite j collide?
			int x_j = visibleSprites[j].x;
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				SpritePattern pattern_j = visibleSprites[j].pattern;
				if (dist < 0) {
					pattern_j <<= -dist;
				} else {
					pattern_j >>= dist;
				}
				if (pattern_i & pattern_j) {
					// Collision!
					status |= 0x20;
					// TODO: Fill in collision coordinates in S#3..S#6.
					//       See page 97 for info.
					// TODO: I guess the VDP checks for collisions while
					//       scanning, if so the top-leftmost collision
					//       should be remembered. Currently the topmost
					//       line is guaranteed, but within that line
					//       the highest sprite numbers are selected.
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

void SpriteChecker::updateSprites0(int limit)
{
	// If this method is called, that means somewhere a check for sprite
	// mode 0 is missing and performance is being wasted.
	// The updateSpritesN methods are called by checkUntil, which is
	// documented as not allowed to be called in sprite mode 0.
	assert(false);
}

void SpriteChecker::updateSprites1(int limit)
{
	while (currentLine < limit) {
		spriteCount[currentLine] =
			checkSprites1(currentLine, spriteBuffer[currentLine]);
		currentLine++;
	}
}

void SpriteChecker::updateSprites2(int limit)
{
	/*
	cout << "updateSprites2: until = " << until
		<< ", frameStart = " << frameStartTime << "\n";
	*/
	while (currentLine < limit) {
		spriteCount[currentLine] =
			checkSprites2(currentLine, spriteBuffer[currentLine]);
		currentLine++;
	}
}

} // namespace openmsx

