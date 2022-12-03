/*
TODO:
- Verify model for 5th sprite number calculation.
  For example, does it have the right value in text mode?
- Further investigate sprite collision registers:
   - If there is NO collision, the value of these registers constantly changes.
     Could this be some kind of indication for the scanline XY coords???
   - Bit 9 of the Y coord (odd/even page??) is not yet implemented.
*/

#include "SpriteChecker.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "serialize.hh"
#include <algorithm>
#include <bit>
#include <cassert>

namespace openmsx {

SpriteChecker::SpriteChecker(VDP& vdp_, RenderSettings& renderSettings,
                             EmuTime::param time)
	: vdp(vdp_), vram(vdp.getVRAM())
	, limitSpritesSetting(renderSettings.getLimitSpritesSetting())
	, frameStartTime(time)
{
	vram.spriteAttribTable.setObserver(this);
	vram.spritePatternTable.setObserver(this);
}

void SpriteChecker::reset(EmuTime::param time)
{
	vdp.setSpriteStatus(0); // TODO 0x00 or 0x1F  (blueMSX has 0x1F)
	collisionX = 0;
	collisionY = 0;

	frameStart(time);

	updateSpritesMethod = &SpriteChecker::updateSprites1;
}

static constexpr SpriteChecker::SpritePattern doublePattern(SpriteChecker::SpritePattern a)
{
	// bit-pattern "abcd...." gets expanded to "aabbccdd"
	// upper 16 bits (of a 32 bit number) contain the pattern
	// lower 16 bits must be zero
	//                               // abcdefghijklmnop0000000000000000
	a = (a | (a >> 8)) & 0xFF00FF00; // abcdefgh00000000ijklmnop00000000
	a = (a | (a >> 4)) & 0xF0F0F0F0; // abcd0000efgh0000ijkl0000mnop0000
	a = (a | (a >> 2)) & 0xCCCCCCCC; // ab00cd00ef00gh00ij00kl00mn00op00
	a = (a | (a >> 1)) & 0xAAAAAAAA; // a0b0c0d0e0f0g0h0i0j0k0l0m0n0o0p0
	return a | (a >> 1);             // aabbccddeeffgghhiijjkkllmmnnoopp
}

inline SpriteChecker::SpritePattern SpriteChecker::calculatePatternNP(
	unsigned patternNr, unsigned y)
{
	auto patternPtr = vram.spritePatternTable.getReadArea<256 * 8>(0);
	unsigned index = patternNr * 8 + y;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp.getSpriteSize() == 16) {
		pattern |= patternPtr[index + 16] << 16;
	}
	return !vdp.isSpriteMag() ? pattern : doublePattern(pattern);
}
inline SpriteChecker::SpritePattern SpriteChecker::calculatePatternPlanar(
	unsigned patternNr, unsigned y)
{
	auto [ptr0, ptr1] = vram.spritePatternTable.getReadAreaPlanar<256 * 8>(0);
	unsigned index = patternNr * 8 + y;
	auto patternPtr = (index & 1) ? ptr1 : ptr0;
	index /= 2;
	SpritePattern pattern = patternPtr[index] << 24;
	if (vdp.getSpriteSize() == 16) {
		pattern |= patternPtr[index + (16 / 2)] << 16;
	}
	return !vdp.isSpriteMag() ? pattern : doublePattern(pattern);
}

void SpriteChecker::updateSprites1(int limit)
{
	if (vdp.spritesEnabledFast()) {
		if (vdp.isDisplayEnabled()) {
			// in display area
			checkSprites1(currentLine, limit);
		} else {
			// in border, only check last line of top border
			int l0 = vdp.getLineZero() - 1;
			if ((currentLine <= l0) && (l0 < limit)) {
				checkSprites1(l0, l0 + 1);
			}
		}
	}
	currentLine = limit;
}

inline void SpriteChecker::checkSprites1(int minLine, int maxLine)
{
	// This implementation contains a double for-loop. The outer loop goes
	// over the sprites, the inner loop over the to-be-checked lines. This
	// is not the order in which the real VDP performs this operation: the
	// real VDP renders line-per-line and for each line checks all 32
	// sprites.
	//
	// Though this 'reverse' order allows to skip over very large regions
	// of the inner loop: we only have to process the lines were a
	// particular sprite is actually visible. I measured this makes this
	// routine 4x-5x faster!
	//
	// This routine also needs to detect the sprite number of the 'first'
	// 5th-sprite-condition. With 'first' meaning the first line where this
	// condition occurs. Because our loops are swapped compared to the real
	// VDP, we need some extra fixup logic to correctly detect this.

	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	int displayDelta = vdp.getVerticalScroll() - vdp.getLineZero();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting.getBoolean();
	int size = vdp.getSpriteSize();
	bool mag = vdp.isSpriteMag();
	int magSize = (mag + 1) * size;
	auto attributePtr = vram.spriteAttribTable.getReadArea<32 * 4>(0);
	byte patternIndexMask = size == 16 ? 0xFC : 0xFF;
	int fifthSpriteNum  = -1;  // no 5th sprite detected yet
	int fifthSpriteLine = 999; // larger than any possible valid line

	int sprite = 0;
	for (/**/; sprite < 32; ++sprite) {
		int y = attributePtr[4 * sprite + 0];
		if (y == 208) break;

		for (int line = minLine; line < maxLine; ++line) { // 'line' changes in loop
			// Calculate line number within the sprite.
			int displayLine = line + displayDelta;
			int spriteLine = (displayLine - y) & 0xFF;
			if (spriteLine >= magSize) {
				// Skip ahead till sprite becomes visible.
				line += 256 - spriteLine - 1; // -1 because of for-loop
				continue;
			}

			auto visibleIndex = spriteCount[line];
			if (visibleIndex == 4) {
				// Find earliest line where this condition occurs.
				if (line < fifthSpriteLine) {
					fifthSpriteLine = line;
					fifthSpriteNum = sprite;
				}
				if (limitSprites) continue;
			}

			SpriteInfo& sip = spriteBuffer[line][visibleIndex];
			int patternIndex = attributePtr[4 * sprite + 2] & patternIndexMask;
			if (mag) spriteLine /= 2;
			sip.pattern = calculatePatternNP(patternIndex, spriteLine);
			sip.x = attributePtr[4 * sprite + 1];
			byte colorAttrib = attributePtr[4 * sprite + 3];
			if (colorAttrib & 0x80) sip.x -= 32;
			sip.colorAttrib = colorAttrib;

			spriteCount[line] = visibleIndex + 1;
		}
	}

	// Update status register.
	byte status = vdp.getStatusReg0();
	if (fifthSpriteNum != -1) {
		// Five sprites on a line.
		// According to TMS9918.pdf 5th sprite detection is only
		// active when F flag is zero.
		if ((status & 0xC0) == 0) {
			status = byte(0x40 | (status & 0x20) | fifthSpriteNum);
		}
	}
	if (~status & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		status = (status & 0x20) | byte(std::min(sprite, 31));
	}
	vdp.setSpriteStatus(status);

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The spriteBuffer array is filled now, so we can bail out.
	if (vdp.getStatusReg0() & 0x20) return;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	- Reset when status reg is read.
	- Set when sprite patterns overlap.
	- ??? Color doesn't matter: sprites of color 0 can collide.
	  ??? This conflicts with: https://github.com/openMSX/openMSX/issues/1198
	- Sprites that are partially off-screen position can collide, but only
	  on the in-screen pixels. In other words: sprites cannot collide in
	  the left or right border, only in the visible screen area. Though
	  they can collide in the V9958 extra border mask. This behaviour is
	  the same in sprite mode 1 and 2.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow,
	but there are max 4 sprites and therefore max 6 pairs.
	If any collision is found, method returns at once.
	*/
	bool can0collide = vdp.canSpriteColor0Collide();
	for (auto line : xrange(minLine, maxLine)) {
		int minXCollision = 999;
		for (int i = std::min<int>(4, spriteCount[line]); --i >= 1; /**/) {
			auto color1 = spriteBuffer[line][i].colorAttrib & 0xf;
			if (!can0collide && (color1 == 0)) continue;
			int x_i = spriteBuffer[line][i].x;
			SpritePattern pattern_i = spriteBuffer[line][i].pattern;
			for (int j = i; --j >= 0; /**/) {
				auto color2 = spriteBuffer[line][j].colorAttrib & 0xf;
				if (!can0collide && (color2 == 0)) continue;
				// Do sprite i and sprite j collide?
				int x_j = spriteBuffer[line][j].x;
				int dist = x_j - x_i;
				if ((-magSize < dist) && (dist < magSize)) {
					SpritePattern pattern_j = spriteBuffer[line][j].pattern;
					if (dist < 0) {
						pattern_j <<= -dist;
					} else {
						pattern_j >>= dist;
					}
					SpritePattern colPat = pattern_i & pattern_j;
					if (x_i < 0) {
						assert(x_i >= -32);
						colPat &= (1 << (32 + x_i)) - 1;
					}
					if (colPat) {
						int xCollision = x_i + std::countl_zero(colPat);
						assert(xCollision >= 0);
						minXCollision = std::min(minXCollision, xCollision);
					}
				}
			}
		}
		if (minXCollision < 256) {
			vdp.setSpriteStatus(vdp.getStatusReg0() | 0x20);
			// verified: collision coords are also filled
			//           in for sprite mode 1
			// x-coord should be increased by 12
			// y-coord                         8
			collisionX = minXCollision + 12;
			collisionY = line - vdp.getLineZero() + 8;
			return; // don't check lines with higher Y-coord
		}
	}
}

void SpriteChecker::updateSprites2(int limit)
{
	// TODO merge this with updateSprites1()?
	if (vdp.spritesEnabledFast()) {
		if (vdp.isDisplayEnabled()) {
			// in display area
			checkSprites2(currentLine, limit);
		} else {
			// in border, only check last line of top border
			int l0 = vdp.getLineZero() - 1;
			if ((currentLine <= l0) && (l0 < limit)) {
				checkSprites2(l0, l0 + 1);
			}
		}
	}
	currentLine = limit;
}

inline void SpriteChecker::checkSprites2(int minLine, int maxLine)
{
	// See comment in checkSprites1() about order of inner and outer loops.

	// Calculate display line.
	// This is the line sprites are checked at; the line they are displayed
	// at is one lower.
	int displayDelta = vdp.getVerticalScroll() - vdp.getLineZero();

	// Get sprites for this line and detect 5th sprite if any.
	bool limitSprites = limitSpritesSetting.getBoolean();
	int size = vdp.getSpriteSize();
	bool mag = vdp.isSpriteMag();
	int magSize = (mag + 1) * size;
	int patternIndexMask = (size == 16) ? 0xFC : 0xFF;
	int ninthSpriteNum  = -1;  // no 9th sprite detected yet
	int ninthSpriteLine = 999; // larger than any possible valid line

	// Because it gave a measurable performance boost, we duplicated the
	// code for planar and non-planar modes.
	int sprite = 0;
	if (planar) {
		auto [attributePtr0, attributePtr1] =
			vram.spriteAttribTable.getReadAreaPlanar<32 * 4>(512);
		// TODO: Verify CC implementation.
		for (/**/; sprite < 32; ++sprite) {
			int y = attributePtr0[2 * sprite + 0];
			if (y == 216) break;

			for (int line = minLine; line < maxLine; ++line) { // 'line' changes in loop
				// Calculate line number within the sprite.
				int displayLine = line + displayDelta;
				int spriteLine = (displayLine - y) & 0xFF;
				if (spriteLine >= magSize) {
					// Skip ahead till sprite is visible.
					line += 256 - spriteLine - 1;
					continue;
				}

				auto visibleIndex = spriteCount[line];
				if (visibleIndex == 8) {
					// Find earliest line where this condition occurs.
					if (line < ninthSpriteLine) {
						ninthSpriteLine = line;
						ninthSpriteNum = sprite;
					}
					if (limitSprites) continue;
				}

				if (mag) spriteLine /= 2;
				unsigned colorIndex = (~0u << 10) | (sprite * 16 + spriteLine);
				byte colorAttrib =
					vram.spriteAttribTable.readPlanar(colorIndex);

				SpriteInfo& sip = spriteBuffer[line][visibleIndex];
				int patternIndex = attributePtr0[2 * sprite + 1] & patternIndexMask;
				sip.pattern = calculatePatternPlanar(patternIndex, spriteLine);
				sip.x = attributePtr1[2 * sprite + 0];
				if (colorAttrib & 0x80) sip.x -= 32;
				sip.colorAttrib = colorAttrib;

				// set sentinel (see below)
				spriteBuffer[line][visibleIndex + 1].colorAttrib = 0;
				spriteCount[line] = visibleIndex + 1;
			}
		}
	} else {
		auto attributePtr0 =
			vram.spriteAttribTable.getReadArea<32 * 4>(512);
		// TODO: Verify CC implementation.
		for (/**/; sprite < 32; ++sprite) {
			int y = attributePtr0[4 * sprite + 0];
			if (y == 216) break;

			for (int line = minLine; line < maxLine; ++line) { // 'line' changes in loop
				// Calculate line number within the sprite.
				int displayLine = line + displayDelta;
				int spriteLine = (displayLine - y) & 0xFF;
				if (spriteLine >= magSize) {
					// Skip ahead till sprite is visible.
					line += 256 - spriteLine - 1;
					continue;
				}

				auto visibleIndex = spriteCount[line];
				if (visibleIndex == 8) {
					// Find earliest line where this condition occurs.
					if (line < ninthSpriteLine) {
						ninthSpriteLine = line;
						ninthSpriteNum = sprite;
					}
					if (limitSprites) continue;
				}

				if (mag) spriteLine /= 2;
				unsigned colorIndex = (~0u << 10) | (sprite * 16 + spriteLine);
				byte colorAttrib =
					vram.spriteAttribTable.readNP(colorIndex);
				// Sprites with CC=1 are only visible if preceded by
				// a sprite with CC=0. However they DO contribute towards
				// the max-8-sprites-per-line limit, so we can't easily
				// filter them here. See also
				//    https://github.com/openMSX/openMSX/issues/497

				SpriteInfo& sip = spriteBuffer[line][visibleIndex];
				int patternIndex = attributePtr0[4 * sprite + 2] & patternIndexMask;
				sip.pattern = calculatePatternNP(patternIndex, spriteLine);
				sip.x = attributePtr0[4 * sprite + 1];
				if (colorAttrib & 0x80) sip.x -= 32;
				sip.colorAttrib = colorAttrib;

				// Set sentinel. Sentinel is actually only
				// needed for sprites with CC=1.
				// In the past we set the sentinel (for all
				// lines) at the end. But it's slightly faster
				// to do it only for lines that actually
				// contain sprites (even if sentinel gets
				// overwritten a couple of times for lines with
				// many sprites).
				spriteBuffer[line][visibleIndex + 1].colorAttrib = 0;
				spriteCount[line] = visibleIndex + 1;
			}
		}
	}

	// Update status register.
	byte status = vdp.getStatusReg0();
	if (ninthSpriteNum != -1) {
		// Nine sprites on a line.
		// According to TMS9918.pdf 5th sprite detection is only
		// active when F flag is zero. Stuck to this for V9938.
		// Dragon Quest 2 needs this.
		if ((status & 0xC0) == 0) {
			status = byte(0x40 | (status & 0x20) | ninthSpriteNum);
		}
	}
	if (~status & 0x40) {
		// No 9th sprite detected, store number of latest sprite processed.
		status = (status & 0x20) | byte(std::min(sprite, 31));
	}
	vdp.setSpriteStatus(status);

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (vdp.getStatusReg0() & 0x20) return;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	- Reset when status reg is read.
	- Set when sprite patterns overlap.
	- ??? Color doesn't matter: sprites of color 0 can collide.
	  ???  TODO: V9938 data book denies this (page 98).
	  ??? This conflicts with: https://github.com/openMSX/openMSX/issues/1198
	- Sprites that are partially off-screen position can collide, but only
	  on the in-screen pixels. In other words: sprites cannot collide in
	  the left or right border, only in the visible screen area. Though
	  they can collide in the V9958 extra border mask. This behaviour is
	  the same in sprite mode 1 and 2.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow.
	There are max 8 sprites and therefore max 42 pairs.
	  TODO: Maybe this is slow... Think of something faster.
	        Probably new approach is needed anyway for OR-ing.
	*/
	bool can0collide = vdp.canSpriteColor0Collide();
	for (auto line : xrange(minLine, maxLine)) {
		int minXCollision = 999; // no collision
		std::span<SpriteInfo, 32 + 1> visibleSprites = spriteBuffer[line];
		for (int i = std::min<int>(8, spriteCount[line]); --i >= 1; /**/) {
			auto colorAttrib1 = visibleSprites[i].colorAttrib;
			if (!can0collide && ((colorAttrib1 & 0xf) == 0)) continue;
			// If CC or IC is set, this sprite cannot collide.
			if (colorAttrib1 & 0x60) continue;

			int x_i = visibleSprites[i].x;
			SpritePattern pattern_i = visibleSprites[i].pattern;
			for (int j = i; --j >= 0; /**/) {
				auto colorAttrib2 = visibleSprites[j].colorAttrib;
				if (!can0collide && ((colorAttrib2 & 0xf) == 0)) continue;
				// If CC or IC is set, this sprite cannot collide.
				if (colorAttrib2 & 0x60) continue;

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
					SpritePattern colPat = pattern_i & pattern_j;
					if (x_i < 0) {
						assert(x_i >= -32);
						colPat &= (1 << (32 + x_i)) - 1;
					}
					if (colPat) {
						int xCollision = x_i + std::countl_zero(colPat);
						assert(xCollision >= 0);
						minXCollision = std::min(minXCollision, xCollision);
					}
				}
			}
		}
		if (minXCollision < 256) {
			vdp.setSpriteStatus(vdp.getStatusReg0() | 0x20);
			// x-coord should be increased by 12
			// y-coord                         8
			collisionX = minXCollision + 12;
			collisionY = line - vdp.getLineZero() + 8;
			return; // don't check lines with higher Y-coord
		}
	}
}

// version 1: initial version
// version 2: bug fix: also serialize 'currentLine'
template<typename Archive>
void SpriteChecker::serialize(Archive& ar, unsigned version)
{
	if constexpr (Archive::IS_LOADER) {
		// Recalculate from VDP state:
		//  - frameStartTime
		frameStartTime.reset(vdp.getFrameStartTime());
		//  - updateSpritesMethod, planar
		setDisplayMode(vdp.getDisplayMode());

		// We don't serialize spriteCount[] and spriteBuffer[].
		// These are only used to draw the MSX screen, they don't have
		// any influence on the MSX state. So the effect of not
		// serializing these two is that no sprites will be shown in the
		// first (partial) frame after loadstate.
		ranges::fill(spriteCount, 0);
		// content of spriteBuffer[] doesn't matter if spriteCount[] is 0
	}
	ar.serialize("collisionX", collisionX,
	             "collisionY", collisionY);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("currentLine", currentLine);
	} else {
		currentLine = 0;
	}
}
INSTANTIATE_SERIALIZE_METHODS(SpriteChecker);

} // namespace openmsx
