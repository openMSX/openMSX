// $Id$

#include "SpriteChecker.hh"
#include "VDPVRAM.hh"
#include <cassert>

SpriteChecker::SpriteChecker(VDP *vdp, bool limitSprites, const EmuTime &time)
	: currentTime(time)
{
	this->vdp = vdp;
	this->limitSprites = limitSprites;
	vram = vdp->getVRAM();

	status = 0;
	collisionX = 0;
	collisionY = 0;
	memset(spriteCount, 0, sizeof(spriteCount));

	updateSpritesMethod = &SpriteChecker::updateSprites1;
}

inline SpriteChecker::SpritePattern SpriteChecker::doublePattern(SpriteChecker::SpritePattern a)
{
	// bit-pattern "abcd...." gets expanded to "aabbccdd"
	a =   a                  | (a>>16);
	a = ((a<< 8)&0x00ffff00) | (a&0xff0000ff);
	a = ((a<< 4)&0x0ff00ff0) | (a&0xf00ff00f);
	a = ((a<< 2)&0x3c3c3c3c) | (a&0xc3c3c3c3);
	a = ((a<< 1)&0x66666666) | (a&0x99999999);
	return a;
}

// TODO Separate planar / non-planar routines.
inline SpriteChecker::SpritePattern SpriteChecker::calculatePattern(
	int patternNr, int y, const EmuTime &time)
{
	if (vdp->getSpriteMag()) y /= 2;
	SpritePattern pattern = vram->cpuRead(
		vdp->getSpritePatternBase() + patternNr * 8 + y, time) << 24;
	if (vdp->getSpriteSize() == 16) {
		pattern |= vram->cpuRead(
			vdp->getSpritePatternBase() + patternNr * 8 + y + 16, time) << 16;
	}
	return (vdp->getSpriteMag() ? doublePattern(pattern) : pattern);
}

inline int SpriteChecker::checkSprites1(
	int line, SpriteChecker::SpriteInfo *visibleSprites, const EmuTime &time)
{
	if (!vdp->spritesEnabled()) return 0;

	// Calculate display line.
	// Compensate for the fact that sprites are calculated one line
	// before they are plotted.
	line = line - vdp->getLineZero() + vdp->getVerticalScroll() - 1;

	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int size = vdp->getSpriteSize();
	int magSize = size * (vdp->getSpriteMag() + 1);
	int attributeBase = vdp->getSpriteAttributeBase();
	const byte *attributePtr = vram->readArea(
		attributeBase, attributeBase + 4 * 32, time);
	for (sprite = 0; sprite < 32; sprite++, attributePtr += 4) {
		int y = *attributePtr;
		if (y == 208) break;
		// Calculate line number within the sprite.
		int spriteLine = (line - y) & 0xFF;
		if (spriteLine < magSize) {
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
			int patternIndex = (size == 16
				? attributePtr[2] & 0xFC : attributePtr[2]);
			sip->pattern = calculatePattern(patternIndex, spriteLine, time);
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
	int line, SpriteChecker::SpriteInfo *visibleSprites, const EmuTime &time)
{
	if (!vdp->spritesEnabled()) return 0;

	// Calculate display line.
	// Compensate for the fact that sprites are calculated one line
	// before they are plotted.
	// TODO: Actually fetch the data one line earlier.
	line = line - vdp->getLineZero() + vdp->getVerticalScroll() - 1;

	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int size = vdp->getSpriteSize();
	int magSize = size * (vdp->getSpriteMag() + 1);
	// TODO: Should masks be applied while processing the tables?
	int colourAddr = vdp->getSpriteAttributeBase() & 0x1FC00;
	int attributeAddr = colourAddr + 512;
	// TODO: Verify CC implementation.
	for (sprite = 0; sprite < 32; sprite++,
			attributeAddr += 4, colourAddr += 16) {
		int y = vram->cpuRead(attributeAddr, time);
		if (y == 216) break;
		// Calculate line number within the sprite.
		int spriteLine = (line - y) & 0xFF;
		if (spriteLine < magSize) {
			if (visibleIndex == 8) {
				// Nine sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero. Stuck to this for V9938.
				if (~status & 0xC0) {
					status = (status & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			byte colourAttrib =
				vram->cpuRead(colourAddr + spriteLine, time);
			// Sprites with CC=1 are only visible if preceded by
			// a sprite with CC=0.
			if ((colourAttrib & 0x40) && visibleIndex == 0) continue;
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex =
				vram->cpuRead(attributeAddr + 2, time);
			// TODO: Precalc pattern index mask.
			if (size == 16) patternIndex &= 0xFC;
			sip->pattern = calculatePattern(patternIndex, spriteLine, time);
			sip->x = vram->cpuRead(attributeAddr + 1, time);
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

void SpriteChecker::updateSprites1(const EmuTime &until)
{
	while (currentTime <= until) {
		// TODO: This happens, but shouldn't.
		//       Something is off between VDP sync points and
		//       VRAM writes by CPU.
		//       Maybe rounding errors due to treatment of
		//       Z80 instructions as atomic?
		if (currentLine >= 313) break;

		spriteCount[currentLine] =
			checkSprites1(currentLine, spriteBuffer[currentLine], until);
		currentLine++;
		currentTime += VDP::TICKS_PER_LINE;
	}
}

void SpriteChecker::updateSprites2(const EmuTime &until)
{
	/*
	cout << "updateSprites2: currentTime = " << currentTime
		<< ", until = " << until
		<< ", frameStart = " << frameStartTime
		<< "\n";
	*/
	while (currentTime <= until) {
		// TODO: This happens, but shouldn't.
		//       Something is off between VDP sync points and
		//       VRAM writes by CPU.
		//       Maybe rounding errors due to treatment of
		//       Z80 instructions as atomic?
		if (currentLine >= 313) break;

		spriteCount[currentLine] =
			checkSprites2(currentLine, spriteBuffer[currentLine], until);
		currentLine++;
		currentTime += VDP::TICKS_PER_LINE;
	}
}

