// $Id$

#ifndef __SPRITECHECKER_HH__
#define __SPRITECHECKER_HH__

#include "EmuTime.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "DisplayMode.hh"

class BooleanSetting;

class SpriteChecker: public VRAMObserver
{
private:
	/** Update sprite checking to specified time.
	  * This includes a VRAM sync.
	  * @param time The moment in emulated time to update to.
	  */
	inline void sync(const EmuTime &time) {
		if (mode0) return;
		// Debug:
		// This method is not re-entrant, so check explicitly that it is not
		// re-entered. This can disappear once the VDP-internal scheduling
		// has become stable.
		static bool syncInProgress = false;
		assert(!syncInProgress);
		syncInProgress = true;
		vram->sync(time);
		checkUntil(time);
		syncInProgress = false;
	}

	inline void initFrame(const EmuTime &time) {
		frameStartTime = time;
		currentLine = 0;
		linesPerFrame = vdp->isPalTiming() ? 313 : 262;
		// Debug: -1 means uninitialised.
		for (int i = 0; i < 313; i++) spriteCount[i] = -1;
		// TODO: Reset anything else? Does the real VDP?
	}

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
	  */
	SpriteChecker(VDP *vdp);

	/** Puts the sprite checker in its initial state.
	  * @param time The moment in time this reset occurs.
	  */
	void reset(const EmuTime &time);

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
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayMode(DisplayMode mode, const EmuTime &time) {
		sync(time);
		switch(mode.getSpriteMode()) {
		case 0:
			updateSpritesMethod = &SpriteChecker::updateSprites0;
			mode0 = true;
			return;
		case 1:
			updateSpritesMethod = &SpriteChecker::updateSprites1;
			break;
		case 2:
			updateSpritesMethod = &SpriteChecker::updateSprites2;
			break;
		default:
			assert(false);
		}
		mode0 = false;
		planar = mode.isPlanar();
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

	/** Update sprite checking until specified line.
	  * VRAM must be up-to-date before this method is called.
	  * It is not allowed to call this method in a spriteless display mode.
	  * @param time The moment in emulated time to update to.
	  */
	inline void checkUntil(const EmuTime &time) {
		// TODO:
		// Currently the sprite checking is done atomically at the end of
		// the display line. In reality, sprite checking is probably done
		// during most of the line. Run tests on real MSX to make a more
		// accurate model of sprite checking.
		int limit = frameStartTime.getTicksTill(time) / VDP::TICKS_PER_LINE;
		if (currentLine < limit) {
			// Call the right update method for the current display mode.
			(this->*updateSpritesMethod)(limit);
		}
	}

	/** Get X coordinate of sprite collision.
	  */
	inline int getCollisionX(const EmuTime &time) {
		sync(time);
		return collisionX;
	}

	/** Get Y coordinate of sprite collision.
	  */
	inline int getCollisionY(const EmuTime &time) {
		sync(time);
		return collisionY;
	}

	/** Reset sprite collision coordinates.
	  * This happens directly after a read, so a timestamp for syncing is
	  * not necessary.
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
		initFrame(time);
	}

	/** Get sprites for a display line.
	  * Returns the contents of the line the last time it was sprite checked;
	  * before getting the sprites, you should sync to a moment in time
	  * after the sprites are checked, or you'll get last frame's sprites.
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
		// Compensate for the fact sprites are checked one line earlier
		// than they are displayed.
		line--;

		// TODO: Is there ever a sprite on absolute line 0?
		//       Maybe there is, but it is never displayed.
		if (line < 0) return 0;

		visibleSprites = spriteBuffer[line];
		assert(spriteCount[line] != -1);
		return spriteCount[line];
	}

	/** Calculate the position of the rightmost 1-bit in a SpritePattern,
	  * this can be used to calculate the effective width of a SpritePattern.
	  * @param pattern The SpritePattern
	  * @return The position of the rightmost 1-bit in the pattern.
	  *   Positions are numbered from left to right.
	  */
	static inline int patternWidth(SpritePattern pattern) {
		// following code is functionally equivalent with
		//     int width = 0;
		//     while(pattern) { width++; pattern<<=1; }
		//     return width;

		int width;
		if (pattern & 0xffff) { width  = 16; pattern &= 0xffff; }
		else                  { width  =  0; pattern >>= 16;    }
		if (pattern & 0x00ff) { width +=  8; pattern &= 0x00ff; }
		else                  {              pattern >>=  8;    }
		if (pattern & 0x000f) { width +=  4; pattern &= 0x000f; }
		else                  {              pattern >>=  4;    }
		if (pattern & 0x0003) { width +=  2; pattern &= 0x0003; }
		else                  {              pattern >>=  2;    }
		//if (a & 0x0001) { width +=  2;              } // slower !!!
		if (pattern & 0x0001) { width +=  1 + (pattern & 1);    }
		else                  { width += (pattern >> 1);        }
		return width;
	}

	// VRAMObserver implementation:

	void updateVRAM(int offset, const EmuTime &time) {
		checkUntil(time);
	}

	void updateWindow(bool enabled, const EmuTime &time) {
		sync(time);
	}

private:
	/** Do not calculate sprite patterns, because this mode is spriteless.
	  */
	void updateSprites0(int limit);

	/** Calculate sprite patterns for sprite mode 1.
	  */
	void updateSprites1(int limit);

	/** Calculate sprite patterns for sprite mode 2.
	  */
	void updateSprites2(int limit);

	/** Doubles a sprite pattern.
	  * @param pattern The pattern to double.
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

	typedef void (SpriteChecker::*UpdateSpritesMethod)(int limit);
	UpdateSpritesMethod updateSpritesMethod;

	/** Number of lines (262/313) in the current frame.
	  */
	int linesPerFrame;

	/** Is the current display mode spriteless?
	  */
	bool mode0;

	/** Is current display mode planar or not?
	  * TODO: Introduce separate update methods for planar/nonplanar modes.
	  */
	bool planar;

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
	BooleanSetting *limitSpritesSetting;

	/** The emulation time when this frame was started (vsync).
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> frameStartTime;

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
