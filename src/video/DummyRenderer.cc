// $Id$

/* TODO:
 * - in RendererFactory: only register this renderer when in CliComm mode
 * - all renderers use SDL at the moment, which means the SDL_VIDEO is inited in
 *   main.cc. This should be done in a superclass of the SDL based renderers.
 *   This will fix the problem that the window isn't destroyed when switching
 *   to the Dummy Renderer, because it will only be initialized and 
 *   deinitialized for SDL based renderers then (in the con/destructor of that 
 *   superclass).
 */


#include "config.h"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RenderSettings.hh"
#include "DummyRenderer.hh"
#include "Scheduler.hh"

namespace openmsx {

DummyRenderer::DummyRenderer(RendererFactory::RendererID id, VDP *vdp)
:
Renderer (id),
vdp (vdp),
vram (vdp->getVRAM()) {
}

DummyRenderer::~DummyRenderer () {
	PRT_DEBUG ("DummyRenderer: Destructing DummyRenderer object");
}

bool DummyRenderer::checkSettings() {
	// First check this is the right renderer.
	if (!Renderer::checkSettings()) return false;
	
	return true;
}

void DummyRenderer::frameStart(const EmuTime &time) {
}

void DummyRenderer::putImage(const EmuTime &time) {
}

void DummyRenderer::putStoredImage() {
}

int DummyRenderer::putPowerOffImage() {
	return 0;
}

void DummyRenderer::updateTransparency(bool enabled, const EmuTime &time) {
}

void DummyRenderer::updateForegroundColour(int colour, const EmuTime &time) {
}

void DummyRenderer::updateBackgroundColour(int colour, const EmuTime &time) {
}

void DummyRenderer::updateBlinkForegroundColour(int colour, const EmuTime &time) {
}

void DummyRenderer::updateBlinkBackgroundColour(int colour, const EmuTime &time) {
}

void DummyRenderer::updateBlinkState(bool enabled, const EmuTime &time) {
}

void DummyRenderer::updatePalette(int index, int grb, const EmuTime &time) {
}

void DummyRenderer::updateVerticalScroll(int scroll, const EmuTime &time) {
}

void DummyRenderer::updateHorizontalScrollLow(byte scroll, const EmuTime &time) {
}

void DummyRenderer::updateHorizontalScrollHigh(byte scroll, const EmuTime &time) {
}

void DummyRenderer::updateBorderMask(bool masked, const EmuTime &time) {
}

void DummyRenderer::updateMultiPage(bool multiPage, const EmuTime &time) {
}

void DummyRenderer::updateHorizontalAdjust(int adjust, const EmuTime &time) {
}

void DummyRenderer::updateDisplayEnabled(bool enabled, const EmuTime &time) {
}

void DummyRenderer::updateDisplayMode(DisplayMode mode, const EmuTime &time) {
}

void DummyRenderer::updateNameBase(int addr, const EmuTime &time) {
}

void DummyRenderer::updatePatternBase(int addr, const EmuTime &time) {
}

void DummyRenderer::updateColourBase(int addr, const EmuTime &time) {
}

void DummyRenderer::updateSpritesEnabled(bool enabled, const EmuTime &time) {
}

void DummyRenderer::updateVRAM(int offset, const EmuTime &time) {
}

void DummyRenderer::updateWindow(bool enabled, const EmuTime &time) {
}

} // namespace openmsx

