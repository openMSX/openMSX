#include "XRenderer.hh"
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include "EmuTime.hh"
#include "SDLEventInserter.hh"
#include <X11/keysym.h>

#define WIDTH 640
#define HEIGHT 480

XRenderer::XRenderer(VDP *vdp, bool fullscreen, const EmuTime &time)
:
Renderer (0), // Full screen is not supported yet
vdp (vdp),
vram (vdp->getVRAM()) {
	if (fullscreen) PRT_INFO ("XRenderer: Full screen is not supported");
	SDL_CreateThread (LoopCaller, this);
}

void XRenderer::EventLoop (void) {
	if (NULL == (X.display = XOpenDisplay (NULL))) {
		PRT_ERROR ("XRenderer: Unable to open display");
		throw MSXException ("Unable to open display"); //not reached
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
	char *nam = "openMSX [ALPHA]";
	XTextProperty name, iconname;
	if (!XStringListToTextProperty(&nam,1,&name)) {
		PRT_INFO ("XRenderer: error setting window name");
	}
	char *iconnam = "openMSX";
	if (!XStringListToTextProperty(&iconnam,1,&iconname)) {
		PRT_INFO ("XRenderer: error setting iconname");
	}
	XSizeHints xsh;
	XClassHint xch;
	XWMHints xwmh;
	xsh.flags = PMinSize | PMaxSize;
	xsh.min_width = xsh.max_width = WIDTH;
	xsh.min_height = xsh.max_height = HEIGHT;
	xch.res_name = "openMSX";
	xch.res_class = "openMSX";
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
				new SDLEventInserter (ev, MSXCPU::instance()->getCurrentTime());
				break;
			default:
				PRT_INFO ("Event of type " << event.type << " not handled");
		}
	}
	if (X.keyboard_state.global_auto_repeat == AutoRepeatModeOn) XAutoRepeatOn (X.display);
	XCloseDisplay (X.display);
}

XRenderer::~XRenderer () {
	PRT_DEBUG ("XRenderer: Destructing XRenderer object");
}

void XRenderer::frameStart(const EmuTime &time) {
}

void XRenderer::putImage(const EmuTime &time) {
}

void XRenderer::setFullScreen(bool) {
}

void XRenderer::updateTransparency(bool enabled, const EmuTime &time) {
}

void XRenderer::updateForegroundColour(int colour, const EmuTime &time) {
}

void XRenderer::updateBackgroundColour(int colour, const EmuTime &time) {
}

void XRenderer::updateBlinkForegroundColour(int colour, const EmuTime &time) {
}

void XRenderer::updateBlinkBackgroundColour(int colour, const EmuTime &time) {
}

void XRenderer::updateBlinkState(bool enabled, const EmuTime &time) {
}

void XRenderer::updatePalette(int index, int grb, const EmuTime &time) {
}

void XRenderer::updateVerticalScroll(int scroll, const EmuTime &time) {
}

void XRenderer::updateHorizontalAdjust(int adjust, const EmuTime &time) {
}

void XRenderer::updateDisplayEnabled(bool enabled, const EmuTime &time) {
}

void XRenderer::updateDisplayMode(int mode, const EmuTime &time) {
}

void XRenderer::updateNameBase(int addr, const EmuTime &time) {
}

void XRenderer::updatePatternBase(int addr, const EmuTime &time) {
}

void XRenderer::updateColourBase(int addr, const EmuTime &time) {
}

void XRenderer::updateVRAM(int addr, byte data, const EmuTime &time) {
}

static const struct {KeySym x; SDLKey sdl;} _keymap[] = {
{'a', SDLK_a}, {'b', SDLK_b}, {'c', SDLK_c}, {'d', SDLK_d}, {'e', SDLK_e},
{'f', SDLK_f}, {'g', SDLK_g}, {'h', SDLK_h}, {'i', SDLK_i}, {'j', SDLK_j},
{'k', SDLK_k}, {'l', SDLK_l}, {'m', SDLK_m}, {'n', SDLK_n}, {'o', SDLK_o},
{'p', SDLK_p}, {'q', SDLK_q}, {'r', SDLK_r}, {'s', SDLK_s}, {'t', SDLK_t},
{'u', SDLK_u}, {'v', SDLK_v}, {'w', SDLK_w}, {'x', SDLK_x}, {'y', SDLK_y},
{'z', SDLK_z}, {XK_F12, SDLK_F12}, {0,} };

std::map <KeySym, SDLKey> *XRenderer::convert::makemap () {
	std::map <KeySym, SDLKey> *retval = new std::map <KeySym, SDLKey> ();
	for (int i = 0; _keymap[i].x; ++i) (*retval)[_keymap[i].x] = _keymap[i].sdl;
	return retval;
}

std::map <KeySym, SDLKey> *XRenderer::convert::keymap = XRenderer::convert::makemap ();
