// $Id$

#ifndef __SDLGLRENDERER_HH__
#define __SDLGLRENDERER_HH__

#include "openmsx.hh"
#include "PixelRenderer.hh"
#include "Display.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "DirtyChecker.hh"
#include "DisplayMode.hh"
#include "GLUtil.hh"


namespace openmsx {

class BooleanSetting;
class Rasterizer;


/** Hi-res (640x480) renderer using OpenGL through SDL.
  */
class SDLGLRenderer : public PixelRenderer
{
public:
	/** Constructor.
	  */
	SDLGLRenderer(
		RendererFactory::RendererID id, VDP* vdp, Rasterizer* rasterizer );

	/** Destructor.
	  */
	virtual ~SDLGLRenderer();

	// Renderer interface:

	virtual void reset(const EmuTime& time);
	virtual bool checkSettings();
	virtual void frameStart(const EmuTime& time);
	//virtual void frameEnd(const EmuTime& time);
	virtual void updateTransparency(bool enabled, const EmuTime& time);
	virtual void updateForegroundColour(int colour, const EmuTime& time);
	virtual void updateBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkForegroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkState(bool enabled, const EmuTime& time);
	virtual void updatePalette(int index, int grb, const EmuTime& time);
	virtual void updateVerticalScroll(int scroll, const EmuTime& time);
	virtual void updateHorizontalAdjust(int adjust, const EmuTime& time);
	//virtual void updateDisplayEnabled(bool enabled, const EmuTime& time);
	virtual void updateDisplayMode(DisplayMode mode, const EmuTime& time);
	virtual void updateNameBase(int addr, const EmuTime& time);
	virtual void updatePatternBase(int addr, const EmuTime& time);
	virtual void updateColourBase(int addr, const EmuTime& time);
	//virtual void updateVRAM(int offset, const EmuTime& time);
	//virtual void updateWindow(bool enabled, const EmuTime& time);

protected:
	virtual void update(const SettingLeafNode* setting);

	void finishFrame();
	void updateVRAMCache(int address);
	void drawBorder(int fromX, int fromY, int limitX, int limitY);
	void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		);
	void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		);

private:
	Rasterizer* rasterizer;

	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif // __SDLGLRENDERER_HH__
