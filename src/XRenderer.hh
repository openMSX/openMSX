// $Id$

#ifndef __XRENDERER_HH__
#define __XRENDERER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
//#include "CharacterConverter.hh"
//#include "BitmapConverter.hh"
#include "VDP.hh"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

class VDP;
class VDPVRAM;
class SpriteChecker;
//class XConsole;

/** Renderer using Xlib
  */
class XRenderer : public Renderer
{
	friend int LoopCaller (void *caller) {
		reinterpret_cast <XRenderer *>(caller)->EventLoop ();
		return 0;
	}
public:
	/** Constructor.
	  * It is suggested to use the createSDLHiRenderer factory
	  * function instead, which automatically selects a colour depth.
	  */
	XRenderer(VDP *vdp, bool fullscreen, const EmuTime &time);

	/** Destructor.
	  */
	virtual ~XRenderer();

	virtual void reset(const EmuTime &time) {} // TODO

	// Renderer interface:

	void frameStart(const EmuTime &time);
	void putImage(const EmuTime &time);
	void setFullScreen(bool enabled);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(int mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	void updateVRAM(int addr, byte data, const EmuTime &time);

private:
	void EventLoop (void);	// new thread
	VDP *vdp;
	VDPVRAM *vram;
	// for clarity Xlib specific variables are put in a struct
	struct {
		Display *display;
		int screen;
		int depth;
		Window window;
		Pixmap pixmap;
		GC gc;
		XKeyboardState keyboard_state;
	} X;
	class convert {
	public:
		convert (KeySym sym) : sym(sym) {}
		operator SDLKey () { return (*keymap)[sym]; }
	private:
		KeySym sym;
		static std::map <KeySym, SDLKey> *makemap ();
		static std::map <KeySym, SDLKey> *keymap;
	};
};

#endif // __XRENDERER_HH__
