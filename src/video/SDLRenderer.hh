// $Id$

#ifndef __SDLRENDERER_HH__
#define __SDLRENDERER_HH__

#include "PixelRenderer.hh"
#include "SettingListener.hh"


namespace openmsx {

class BooleanSetting;
class Rasterizer;


/** Renderer on SDL surface.
  */
class SDLRenderer : public PixelRenderer
{
public:
	/** Constructor.
	  */
	SDLRenderer(
		RendererFactory::RendererID id, VDP* vdp, Rasterizer* rasterizer );

	/** Destructor.
	  */
	virtual ~SDLRenderer();

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
	// PixelRenderer:
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

	// SettingListener interface:
	virtual void update(const SettingLeafNode* setting);

private:
	Rasterizer* rasterizer;

	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif //__SDLRENDERER_HH__
