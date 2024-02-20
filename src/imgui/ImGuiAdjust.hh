#ifndef IMGUI_ADJUST_HH
#define IMGUI_ADJUST_HH

#include "gl_vec.hh"

namespace openmsx {

// Keep the imgui window in the 'same' position in the main view port (= the
// main openMSX window) when that view port changes size. This happens when
// changing scale_factor, but also when switching to/from fullscreen.
//
// Without this adjustment, after the resize, the window can e.g. move from the
// border to somewhere in the middle of the view port, or even outside the view
// port. This is not desired for stuff like the OSD icons or the reverse-bar.
//
// The heuristic for keeping the window in the 'same' position is:
// * Only make adjustment when the window is shown fully inside the main view port.
// * Separately for the horizontal and vertical direction:
// * Check if the window is closer to the left or the right (or top or bottom)
//   border of the viewport.
// * Keep this closet distance the same after rescaling. In other words, if the
//   window was e.g. close to the bottom, it will remain near the bottom.
// * And some stuff for boundary conditions, see comments in the code for details.
class AdjustWindowInMainViewPort
{
public:
	// should be called right before the call to im::Window()
	void pre();

	// should be called right after the call to im::Window()
	bool post();

private:
	gl::vec2 oldViewPortSize;
	gl::vec2 oldRelWindowPos;
	bool oldIsOnMainViewPort = false;
	bool setMainViewPort = false;
};

} // namespace openmsx

#endif
