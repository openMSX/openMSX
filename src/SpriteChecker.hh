// $Id$

#ifndef __SPRITECHECKER_HH__
#define __SPRITECHECKER_HH__

/*
TODO:
- Should there be a reset() method?
  Or is it good enough to just call frameStart()?
  Currently not: the constructor resets things frameStart() doesn't.
*/

#include "EmuTime.hh"
#include "VDP.hh"

class VDPVRAM;

class SpriteChecker
{
public:

	/** Bitmap of length 32 describing a sprite pattern.
	  * Visible pixels are 1, transparent pixels are 0.
	  * If the sprite is less than 32 pixels wide,
	  * the lower bits are unused.
	  */
	typedef unsigned int SpritePattern;

	/** Contains all the information to draw a line of a sprite.
	  */
	typedef struct {
		/** Pattern of this sprite line, corrected for magnification.
		  */
		SpritePattern pattern;
		/** X-coordinate of sprite, corrected for early clock.
		  */
		short int x;
		/** Bit 3..0 are index in palette.
		  * Bit 6 is 0 for sprite mode 1 like behaviour,
		  * or 1 for OR-ing of sprite colours.
		  * Other bits are undefined.
		  */
		byte colourAttrib;
	} SpriteInfo;

	/** Create a sprite checker.
	  * @param vdp The VDP this sprite checker is part of.
	  * @param limitSprites If true, limit number of sprites per line
	  *   as real VDP does. If false, display all sprites.
	  * @param time Moment in emulated time sprite checking is started.
	  */
	SpriteChecker(VDP *vdp, bool limitSprites, const EmuTime &time);

	/** Gets the sprite status (part of S#0).
	  * Bit 7 (F) is zero; it is not sprite dependant.
	  * Bit 6 (5S) is set when more than 4 (sprite mode 1) or 8 (sprite
	  *   mode 2) sprites occur on the same line.
	  * Bit 5 (C) is set when sprites collide.
	  * Bit 4..0 (5th sprite number) contains the number of the first
	  *   sprite to exceed the limit per line.
	  * Reading the status resets some of the bits.
	  */
	inline byte readStatus(const EmuTime &time) {
		sync(time);
		byte ret = status;
		// TODO: Used to be 0x5F, but that is contradicted by
		//       TMS9918.pdf. Check on real MSX.
		status &= 0x1F;
		return ret;
	}

	/** Informs the sprite checker of a VDP display mode change.
	  * @param mode The new display mode: M5..M1.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayMode(int mode, const EmuTime &time) {
		sync(time);
		if (mode < 8) {
			updateSpritesMethod = &SpriteChecker::updateSprites1;
		} else {
			updateSpritesMethod = &SpriteChecker::updateSprites2;
		}
	}

	/** Informs the sprite checker of a VDP display enabled change.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayEnabled(bool enabled, const EmuTime &time) {
		sync(time);
		// TODO: Speed up sprite checking in display disabled case.
	}

	/** Informs the sprite checker of sprite enable changes.
	  * @param enabled The new sprite enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpritesEnabled(bool enabled, const EmuTime &time) {
		sync(time);
		// TODO: Speed up sprite checking in display disabled case.
	}

	/** Informs the sprite checker of sprite size or magnification changes.
	  * @param sizeMag The new size and magnification state.
	  *   Bit 0 is magnification: 0 = normal, 1 = doubled.
	  *   Bit 1 is size: 0 = 8x8, 1 = 16x16.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpriteSizeMag(byte sizeMag, const EmuTime &time) {
		sync(time);
		// TODO: Precalc something?
	}

	/** Informs the sprite checker of a vertical scroll change.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateVerticalScroll(int scroll, const EmuTime &time) {
		sync(time);
		// TODO: Precalc something?
	}

	/** Informs the sprite checker of an attribute table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpriteAttributeBase(int addr, const EmuTime &time) {
		sync(time);
		// TODO: Update VRAM window range.
	}

	/** Informs the sprite checker of a pattern table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpritePatternBase(int addr, const EmuTime &time) {
		sync(time);
		// TODO: Update VRAM window range.
	}

	inline void checkUntil(const EmuTime &time) {
		// This calls either updateSprites1 or updateSprites2,
		// depending on the current DisplayMode.
		int limit =
			(frameStartTime.getTicksTill(time) / VDP::TICKS_PER_LINE) + 1;
		int linesPerFrame = vdp->isPalTiming() ? 313 : 262;
		if (limit == linesPerFrame + 1) {
			// Sprite memory is read one line in advance.
			// This doesn't integrate nicely with frameStart() yet:
			// line counter is reset at frame start, while line 0 sprite
			// data is read at last line of previous frame.
			// As a workaround, we read line 0 data at line 0 time,
			// all other lines are read one line in advance as it should be.
			limit--;
		} else if (limit > linesPerFrame) {
			cout << "checkUntil: limit = " << limit << "\n";
			assert(false);
		}
		(this->*updateSpritesMethod)(limit);
	}

	/** Informs the sprite checker of a change in VRAM contents.
	  * @param addr The address that will change.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateVRAM(int addr, byte data, const EmuTime &time) {
		checkUntil(time);
	}

	/** Get X coordinate of sprite collision.
	  */
	inline int getCollisionX() { return collisionX; }

	/** Get Y coordinate of sprite collision.
	  */
	inline int getCollisionY() { return collisionY; }

	/** Reset sprite collision coordinates.
	  */
	inline void resetCollision() {
		collisionX = collisionY = 0;
	}

	/** Signals the start of a new frame.
	  * @param time Moment in emulated time the new frame starts.
	  */
	inline void frameStart(const EmuTime &time) {
		// Finish old frame.
		sync(time);
		// Init new frame.
		frameStartTime = time;
		currentLine = 0;
		// TODO: Reset anything else? Does the real VDP?

		for (int i = 0; i < 313; i++) spriteCount[i] = -1;
	}

	/** Get sprites for a display line.
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The absolute line number for which sprites should
	  *   be returned. Range is [0..313) for PAL and [0..262) for NTSC.
	  * @param visibleSprites Output parameter in which the pointer to
	  *   a SpriteInfo array containing the sprites to be displayed is
	  *   returned.
	  *   The array's contents are valid until the next time the VDP
	  *   is scheduled.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int getSprites(int line, SpriteInfo *&visibleSprites) {
		EmuTime time = frameStartTime + line * VDP::TICKS_PER_LINE;
		if (line >= currentLine) {
			/*
			cout << "performing extra updateSprites: "
				<< "old line = " << (currentLine - 1)
				<< ", new line = " << line
				<< "\n";
			*/
			//sync(time);
			// Current VRAM state is good, no need to sync with VRAM.
			checkUntil(time);
			//assert(line < currentLine);
		}
		visibleSprites = spriteBuffer[line];
		//assert(spriteCount[line] != -1);
		if (spriteCount[line] < 0) return 0;
		return spriteCount[line];
	}

private:
	/** Update sprite checking to specified time.
	  * @param time The moment in emulated time to update to.
	  * TODO: Inline would be nice, but there are cyclic dependencies
	  *       with VDPVRAM.
	  */
	void sync(const EmuTime &time);

	/** Calculate sprite pattern for sprite mode 1.
	  */
	void updateSprites1(int limit);

	/** Calculate sprite pattern for sprite mode 2.
	  */
	void updateSprites2(int limit);

	typedef void (SpriteChecker::*UpdateSpritesMethod)(int limit);
	UpdateSpritesMethod updateSpritesMethod;

	/** Doubles a sprite pattern.
	  */
	inline SpritePattern doublePattern(SpritePattern pattern);

	/** Calculates a sprite pattern.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the leftmost bit of the sprite.
	  *   Unused bits are zero.
	  */
	inline SpritePattern calculatePattern(int patternNr, int y);

	/** Calculates a sprite pattern.
	  * Ignores planar addressing.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the leftmost bit of the sprite.
	  *   Unused bits are zero.
	  */
	inline SpritePattern calculatePatternNP(int patternNr, int y);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 1 (MSX1).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites1(int line, SpriteInfo *visibleSprites);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 2 (MSX2).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites2(int line, SpriteInfo *visibleSprites);

	/** The VDP this sprite checker is part of.
	  */
	VDP *vdp;

	/** The VRAM to get sprites data from.
	  */
	VDPVRAM *vram;

	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	bool limitSprites;

	/** The emulation time when this frame was started (vsync).
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> frameStartTime;

	/** The emulation time up to when sprite checking was performed.
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

	/** Sprites are checked up to and excluding this display line.
	  * TODO: Keep this now that we have currentTime?
	  */
	int currentLine;

	/** The sprite contribution to VDP status register 0.
	  */
	byte status;

	/** X coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
	  */
	int collisionX;

	/** Y coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
	  * Bit 9 contains EO, I guess that's a copy of the even/odd flag
	  * of the frame on which the collision occurred.
	  */
	int collisionY;

	/** Buffer containing the sprites that are visible on each
	  * display line.
	  */
	SpriteInfo spriteBuffer[313][32];

	/** Buffer containing the number of sprites that are visible
	  * on each display line.
	  * In other words, spriteCount[i] is the number of sprites
	  * in spriteBuffer[i].
	  */
	int spriteCount[313];

};

#endif // __SPRITECHECKER_HH__
