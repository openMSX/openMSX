// $Id$

#ifndef __DUMMYRENDERER_HH__
#define __DUMMYRENDERER_HH__

#include "config.h"

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDP.hh"

namespace openmsx {

class VDP;
class VDPVRAM;
class SpriteChecker;

/** Dummy Renderer
  */
class DummyRenderer : public Renderer
{
public:


	// Renderer interface:

	void reset(const EmuTime &time) {} // TODO
	bool checkSettings();
	void frameStart(const EmuTime &time);
	void putImage(const EmuTime &time, bool store);
	void putStoredImage();
	int putPowerOffImage();
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalScrollLow(byte scroll, const EmuTime &time);
	void updateHorizontalScrollHigh(byte scroll, const EmuTime &time);
	void updateBorderMask(bool masked, const EmuTime &time);
	void updateMultiPage(bool multiPage, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(DisplayMode mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	void updateSpritesEnabled(bool enabled, const EmuTime &time);
	void updateVRAM(int offset, const EmuTime &time);
	void updateWindow(bool enabled, const EmuTime &time);

private:

	friend class DummyRendererFactory;

	/** Constructor, called by DummyRendererFactory.
	  */
	DummyRenderer(RendererFactory::RendererID id, VDP *vdp);

	/** Destructor.
	  */
	virtual ~DummyRenderer();
	
	VDP *vdp;
	VDPVRAM *vram;
};

} // namespace openmsx

#endif // __DUMMYRENDERER_HH__
