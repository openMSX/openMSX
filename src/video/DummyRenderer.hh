// $Id$

#ifndef __DUMMYRENDERER_HH__
#define __DUMMYRENDERER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "Display.hh"


namespace openmsx {

/** Dummy Renderer
  */
class DummyRenderer : public Renderer, public Layer
{
public:
	// Renderer interface:

	void reset(const EmuTime& /*time*/) {} // TODO
	void frameStart(const EmuTime& time);
	void frameEnd(const EmuTime& time);
	void updateTransparency(bool enabled, const EmuTime& time);
	void updateForegroundColour(int colour, const EmuTime& time);
	void updateBackgroundColour(int colour, const EmuTime& time);
	void updateBlinkForegroundColour(int colour, const EmuTime& time);
	void updateBlinkBackgroundColour(int colour, const EmuTime& time);
	void updateBlinkState(bool enabled, const EmuTime& time);
	void updatePalette(int index, int grb, const EmuTime& time);
	void updateVerticalScroll(int scroll, const EmuTime& time);
	void updateHorizontalScrollLow(byte scroll, const EmuTime& time);
	void updateHorizontalScrollHigh(byte scroll, const EmuTime& time);
	void updateBorderMask(bool masked, const EmuTime& time);
	void updateMultiPage(bool multiPage, const EmuTime& time);
	void updateHorizontalAdjust(int adjust, const EmuTime& time);
	void updateDisplayEnabled(bool enabled, const EmuTime& time);
	void updateDisplayMode(DisplayMode mode, const EmuTime& time);
	void updateNameBase(int addr, const EmuTime& time);
	void updatePatternBase(int addr, const EmuTime& time);
	void updateColourBase(int addr, const EmuTime& time);
	void updateSpritesEnabled(bool enabled, const EmuTime& time);
	void updateVRAM(unsigned offset, const EmuTime& time);
	void updateWindow(bool enabled, const EmuTime& time);
	virtual float getFrameRate() const;

	// Layer interface:
	virtual void paint();
	virtual const string& getName();

private:
	friend class RendererFactory;

	/** Constructor, called by RendererFactory.
	  */
	DummyRenderer();

	/** Destructor.
	  */
	virtual ~DummyRenderer();
};

} // namespace openmsx

#endif // __DUMMYRENDERER_HH__
