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

private:
	/** Calculate sprite pattern for sprite mode 1.
	  */
	void updateSprites1(const EmuTime &until);

	/** Calculate sprite pattern for sprite mode 2.
	  */
	void updateSprites2(const EmuTime &until);

	typedef void (SpriteChecker::*UpdateSpritesMethod)
		(const EmuTime &until);
	UpdateSpritesMethod updateSpritesMethod;
	
public:
	/** Update sprite checking to specified time.
	  * @param time The moment in emulated time to update to.
	  */
	inline void sync(const EmuTime &time) {
		// This calls either updateSprites1 or updateSprites2
		// depending on the current DisplayMode
		(this->*updateSpritesMethod)(time);
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
		spriteLine = 0;
		// TODO: Reset anything else? Does the real VDP?
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
		if (line >= spriteLine) {
			/*
			cout << "performing extra updateSprites: "
				<< "old line = " << (spriteLine - 1)
				<< ", new line = " << line
				<< "\n";
			*/
			sync(time);
		}
		visibleSprites = spriteBuffer[line];
		return spriteCount[line];
	}

private:

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
	inline SpritePattern calculatePattern(int patternNr, int y, const EmuTime &time);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 1 (MSX1).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites1(int line, SpriteInfo *visibleSprites, const EmuTime &time);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 2 (MSX2).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites2(int line, SpriteInfo *visibleSprites, const EmuTime &time);

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
	int spriteLine;

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
