// $Id$

#include "probed_defs.hh"
#ifdef	HAVE_X11

#include "XRenderer.hh"
#include <SDL.h>
#include <SDL_thread.h>
#include "SDLEventInserter.hh"
#include <X11/keysym.h>
#include "Scheduler.hh"
#include "CliCommOutput.hh"

#define WIDTH 640
#define HEIGHT 480

namespace openmsx {

XRenderer::XRenderer(RendererFactory::RendererID id, VDP* vdp)
	: Renderer (id)
	, vdp (vdp)
	, vram (vdp->getVRAM())
{
	SDL_CreateThread (LoopCaller, this);
}

void XRenderer::EventLoop()
{
	if (NULL == (X.display = XOpenDisplay (NULL))) {
		throw FatalError("XRenderer: Unable to open display");
	}
	X.screen = DefaultScreen (X.display);
	X.depth = DefaultDepth (X.display, X.screen);
	X.window = XCreateSimpleWindow (
		X.display, RootWindow (X.display, X.screen),
		0, 0, WIDTH, HEIGHT, //geometry
		1, BlackPixel (X.display, X.screen), //border
		WhitePixel (X.display, X.screen)); //background
	X.pixmap = XCreatePixmap (X.display, X.window, WIDTH, HEIGHT, X.depth);
	XGCValues vals;
	vals.font = XLoadFont (X.display, "fixed"); // a font seems to be needed
	vals.foreground = BlackPixel (X.display, X.screen);
	vals.background = WhitePixel (X.display, X.screen);
	X.gc = XCreateGC (X.display, RootWindow (X.display, X.screen),
		GCFont | GCForeground | GCBackground, &vals);
	char *nam = const_cast<char*>("openMSX [ALPHA]");
	XTextProperty name, iconname;
	if (!XStringListToTextProperty(&nam, 1, &name)) {
		CliCommOutput::instance().printWarning (
			"XRenderer: error setting window name");
	}
	char *iconnam = const_cast<char*>("openMSX");
	if (!XStringListToTextProperty(&iconnam, 1, &iconname)) {
		CliCommOutput::instance().printWarning (
			"XRenderer: error setting iconname");
	}
	XSizeHints xsh;
	XClassHint xch;
	XWMHints xwmh;
	xsh.flags = PMinSize | PMaxSize;
	xsh.min_width = xsh.max_width = WIDTH;
	xsh.min_height = xsh.max_height = HEIGHT;
	xch.res_name  = const_cast<char*>("openMSX");
	xch.res_class = const_cast<char*>("openMSX");
	xwmh.flags = StateHint | InputHint;
	xwmh.input = True;
	xwmh.initial_state = NormalState;
	char *argv[] = {0};
	XSetWMProperties (X.display, X.window, &name, &iconname, argv, 0,
		&xsh, &xwmh, &xch);
	XSelectInput (X.display, X.window, ExposureMask | KeyPressMask
		| KeyReleaseMask | EnterWindowMask | LeaveWindowMask);
	XMapWindow (X.display, X.window);
	XFlush (X.display);
	XEvent event;
	XGetKeyboardControl (X.display, &X.keyboard_state);
	while (true) {
		XNextEvent (X.display, &event);
		PRT_DEBUG ("XRenderer: received event " << event.type);
		switch (event.type) {
			case Expose:
				XCopyArea (X.display, X.pixmap, X.window, X.gc,
					event.xexpose.x, event.xexpose.y,
					event.xexpose.width, event.xexpose.height,
					event.xexpose.x, event.xexpose.y);
				break;
			case EnterNotify:
				XAutoRepeatOff (X.display);
				break;
			case LeaveNotify:
				if (X.keyboard_state.global_auto_repeat == AutoRepeatModeOn) XAutoRepeatOn (X.display);
				break;
			case KeyPress:
			case KeyRelease:
				SDL_Event ev;
				ev.type = (event.type == KeyPress ? SDL_KEYDOWN : SDL_KEYUP);
// These fields are not used so they need not to be filled
// But it doesn't harm to fill them anyway...
				ev.key.state = (event.type == KeyPress ? SDL_PRESSED : SDL_RELEASED);
				//ev.key.scancode = event.XKeyEvent.keycode;
// Without any good reason, I assume SDL keycodes are the same as X keycodes.
				ev.key.keysym.sym = convert(XKeycodeToKeysym (X.display, event.xkey.keycode, 0));
				PRT_DEBUG ("XRenderer: Inserting KeyEvent in SDL queue");
				new SDLEventInserter (ev, Scheduler::ASAP);
				break;
			default:
				PRT_DEBUG("Event of type " << event.type << " not handled");
		}
	}
	if (X.keyboard_state.global_auto_repeat == AutoRepeatModeOn) XAutoRepeatOn (X.display);
	XCloseDisplay (X.display);
}

XRenderer::~XRenderer ()
{
	PRT_DEBUG ("XRenderer: Destructing XRenderer object");
}

bool XRenderer::checkSettings()
{
	// First check this is the right renderer.
	if (!Renderer::checkSettings()) return false;
	
	// TODO: Check other settings, such as full screen.
	return true;
}

void XRenderer::frameStart(const EmuTime& time) {
}

void XRenderer::frameEnd(const EmuTime& time) {
}

void XRenderer::updateTransparency(bool enabled, const EmuTime& time) {
}

void XRenderer::updateForegroundColour(int colour, const EmuTime& time) {
}

void XRenderer::updateBackgroundColour(int colour, const EmuTime& time) {
}

void XRenderer::updateBlinkForegroundColour(int colour, const EmuTime& time) {
}

void XRenderer::updateBlinkBackgroundColour(int colour, const EmuTime& time) {
}

void XRenderer::updateBlinkState(bool enabled, const EmuTime& time) {
}

void XRenderer::updatePalette(int index, int grb, const EmuTime& time) {
}

void XRenderer::updateVerticalScroll(int scroll, const EmuTime& time) {
}

void XRenderer::updateHorizontalScrollLow(byte scroll, const EmuTime& time) {
}

void XRenderer::updateHorizontalScrollHigh(byte scroll, const EmuTime& time) {
}

void XRenderer::updateBorderMask(bool masked, const EmuTime& time) {
}

void XRenderer::updateMultiPage(bool multiPage, const EmuTime& time) {
}

void XRenderer::updateHorizontalAdjust(int adjust, const EmuTime& time) {
}

void XRenderer::updateDisplayEnabled(bool enabled, const EmuTime& time) {
}

void XRenderer::updateDisplayMode(DisplayMode mode, const EmuTime& time) {
}

void XRenderer::updateNameBase(int addr, const EmuTime& time) {
}

void XRenderer::updatePatternBase(int addr, const EmuTime& time) {
}

void XRenderer::updateColourBase(int addr, const EmuTime& time) {
}

void XRenderer::updateSpritesEnabled(bool enabled, const EmuTime& time) {
}

void XRenderer::updateVRAM(int offset, const EmuTime& time) {
}

void XRenderer::updateWindow(bool enabled, const EmuTime& time) {
}

static const struct {KeySym x; SDLKey sdl;} _keymap[] = {
{'a', SDLK_a}, {'b', SDLK_b}, {'c', SDLK_c}, {'d', SDLK_d}, {'e', SDLK_e},
{'f', SDLK_f}, {'g', SDLK_g}, {'h', SDLK_h}, {'i', SDLK_i}, {'j', SDLK_j},
{'k', SDLK_k}, {'l', SDLK_l}, {'m', SDLK_m}, {'n', SDLK_n}, {'o', SDLK_o},
{'p', SDLK_p}, {'q', SDLK_q}, {'r', SDLK_r}, {'s', SDLK_s}, {'t', SDLK_t},
{'u', SDLK_u}, {'v', SDLK_v}, {'w', SDLK_w}, {'x', SDLK_x}, {'y', SDLK_y},
{'z', SDLK_z}, {XK_F12, SDLK_F12}, {0,} };

map<KeySym, SDLKey> *XRenderer::convert::makemap()
{
	map<KeySym, SDLKey> *retval = new map<KeySym, SDLKey>();
	for (int i = 0; _keymap[i].x; ++i) {
		(*retval)[_keymap[i].x] = _keymap[i].sdl;
	}
	return retval;
}

map<KeySym, SDLKey> *XRenderer::convert::keymap = XRenderer::convert::makemap();

} // namespace openmsx

#endif	// HAVE_X11
