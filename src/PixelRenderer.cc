// $Id$

#include "PixelRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include "CommandController.hh"

/*
TODO:
- Move accuracy handling here. (from Renderer).
- Move full screen handling here? (from Renderer)
- Is it possible to do some for of dirty checking here?
  And is it a good idea?
*/

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

inline void PixelRenderer::draw(
	int startX, int startY, int endX, int endY, DrawType drawType )
{
	switch(drawType) {
	case DRAW_BORDER:
		drawBorder(startX, startY, endX, endY);
		break;
	case DRAW_DISPLAY:
		drawDisplay(startX, startY, endX, endY);
		break;
	default:
		assert(false);
	}
}

inline void PixelRenderer::subdivide(
	int startX, int startY, int endX, int endY, int clipL, int clipR,
	DrawType drawType )
{
	// Partial first line.
	if (startX > clipL) {
		if (startX < clipR) {
			draw(
				startX, startY,
				( startY == endY && endX < clipR
				? endX
				: clipR
				), startY + 1,
				drawType
				);
		}
		if (startY == endY) return;
		startY++;
	}
	// Partial last line.
	bool drawLast = false;
	if (endX >= clipR) {
		endY++;
	} else if (endX > clipL) {
		drawLast = true;
	}
	// Full middle lines.
	if (startY < endY) {
		draw(clipL, startY, clipR, endY, drawType);
	}
	// Actually draw last line if necessary.
	// The point of keeping top-to-bottom draw order is that it increases
	// the locality of memory references, which generally improves cache
	// hit rates.
	if (drawLast) draw(clipL, endY, endX, endY + 1, drawType);
}

inline void PixelRenderer::renderUntil(const EmuTime &time)
{
	if (curFrameSkip != 0) return;

	int limitTicks = vdp->getTicksThisFrame(time);
	int displayL = getDisplayLeft();
	int limitX, limitY;
	switch (accuracy) {
	case ACC_PIXEL: {
		limitX = limitTicks % VDP::TICKS_PER_LINE;
		limitY = limitTicks / VDP::TICKS_PER_LINE;
		break;
	}
	case ACC_LINE: {
		limitX = 0;
		limitY = (limitTicks + VDP::TICKS_PER_LINE - displayL)
			/ VDP::TICKS_PER_LINE;
		if (limitY == nextY) return;
		break;
	}
	case ACC_SCREEN: {
		// TODO: Implement.
		return;
	}
	default:
		assert(false);
	}

	// Note: Rounding errors might push limitTicks beyond
	//       #lines * TICKS_PER_LINE, but never more than a line beyond.
	// TODO: Get rid of rounding errors altogether.
	assert(limitY <= (vdp->isPalTiming() ? 313 : 262));

	if (displayEnabled) {
		// Calculate end of display in ticks since start of line.
		int displayR = displayL + (vdp->isTextMode() ? 960 : 1024);

		// Left border.
		subdivide(nextX, nextY, limitX, limitY,
			0, displayL, DRAW_BORDER );
		// Display area.
		subdivide(nextX, nextY, limitX, limitY,
			displayL, displayR, DRAW_DISPLAY );
		// Right border.
		subdivide(nextX, nextY, limitX, limitY,
			displayR, VDP::TICKS_PER_LINE, DRAW_BORDER );
	} else {
		subdivide(nextX, nextY, limitX, limitY,
			0, VDP::TICKS_PER_LINE, DRAW_BORDER );
	}

	nextX = limitX;
	nextY = limitY;
}

PixelRenderer::PixelRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
	: Renderer(fullScreen), frameSkipCmd(this)
{
	this->vdp = vdp;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();

	frameSkip = curFrameSkip = 0;
	autoFrameSkip = false;
	CommandController::instance()->registerCommand(frameSkipCmd, "frameskip");

	// Now we're ready to start rendering the first frame.
	displayEnabled = false;
	frameStart(time);
}

PixelRenderer::~PixelRenderer()
{
	CommandController::instance()->unregisterCommand("frameskip");
}

void PixelRenderer::updateDisplayEnabled(bool enabled, const EmuTime &time)
{
	sync(time);
	displayEnabled = enabled;
}

void PixelRenderer::frameStart(const EmuTime &time)
{
	//cerr << "timing: " << (vdp->isPalTiming() ? "PAL" : "NTSC") << "\n";

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	// TODO: Use screen lines instead.
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	// Calculate important moments in frame rendering.
	lineBottomErase = vdp->isPalTiming() ? 313 - 3 : 262 - 3;
	nextX = 0;
	nextY = 0;

	if (--curFrameSkip < 0) {
		curFrameSkip = frameSkip;
	}
}

void PixelRenderer::putImage(const EmuTime &time)
{
	// Render changes from this last frame.
	sync(time);

	// Let underlying graphics system finish rendering this frame.
	finishFrame();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	float factor=RealTime::instance()->sync();
	if ( autoFrameSkip ){
					
		if (factor > 1.0  && frameSkip < 30){
			// if sync indicates we are to slow and we have been to
			// slow for X screen then frameSkip++
			// increasing frameskip should be tried rather fast
			if (++frameSkipToLow > 5){
				frameSkipToLow=0;
				frameSkipToHigh=0;
				frameSkip++;
			}
		} else if (factor < 0.65 ){
			// else if sync indicates we are fast enough and we have
			// been to fast enough for Y screens then frameSkip--
			//
			// once we are "on speed" we want to maintain this
			// state long enough before trying to decrase frameSkip
			// otherwise we need again X screens to get the correct
			// frameskip again
			if (frameSkip && (++frameSkipToHigh > 100) ){
				frameSkipToLow=0;
				frameSkipToHigh=0;
				frameSkip--;
			}
		}

	};
}


// Frame skip command
PixelRenderer::FrameSkipCmd::FrameSkipCmd(PixelRenderer *rend)
{
	renderer = rend;
}
void PixelRenderer::FrameSkipCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 1: {
		char message[100];
		if ( renderer->autoFrameSkip ){
			sprintf(message, "Self configurating frame skip (current %d)", renderer->frameSkip);
		} else {
			sprintf(message, "Frame skip: %d", renderer->frameSkip);
		};
		print(std::string(message));
		break;
	}
	case 2: {
		if ( 0 == strcasecmp(tokens[1].c_str(),"auto")) {
			renderer->autoFrameSkip=true;
		} else {
			renderer->autoFrameSkip=false;
			int tmp = strtol(tokens[1].c_str(), NULL, 0);
			if (tmp >= 0) {
				renderer->frameSkip = tmp;
				renderer->curFrameSkip = 0;
			} else {
				throw CommandException("Illegal argument");
			}
		}
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void PixelRenderer::FrameSkipCmd::help(const std::vector<std::string> &tokens)
{
	print("This command sets the frameskip setting");
	print(" frameskip:       displays the current setting");
	print(" frameskip <num>: set the amount of frameskip, 0 is no skips");
	print(" frameskip auto : set frameskip depending on CPU usage");
}

