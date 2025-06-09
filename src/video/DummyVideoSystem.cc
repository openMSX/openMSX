#include "DummyVideoSystem.hh"
#include "Rasterizer.hh"
#include "V9990Rasterizer.hh"
#include "LDRasterizer.hh"
#include "components.hh"
#include "unreachable.hh"

namespace openmsx {

std::unique_ptr<Rasterizer> DummyVideoSystem::createRasterizer(VDP& /*vdp*/)
{
	UNREACHABLE;
}

std::unique_ptr<V9990Rasterizer> DummyVideoSystem::createV9990Rasterizer(
	V9990& /*vdp*/)
{
	UNREACHABLE;
}

#if COMPONENT_LASERDISC
std::unique_ptr<LDRasterizer> DummyVideoSystem::createLDRasterizer(
	LaserdiscPlayer& /*ld*/)
{
	UNREACHABLE;
}
#endif

void DummyVideoSystem::flush()
{
}

std::optional<gl::ivec2> DummyVideoSystem::getMouseCoord()
{
	return {};
}

OutputSurface* DummyVideoSystem::getOutputSurface()
{
	return nullptr;
}

void DummyVideoSystem::showCursor(bool /*show*/)
{
}

bool DummyVideoSystem::getCursorEnabled()
{
	return false;
}

std::string DummyVideoSystem::getClipboardText()
{
	return "";
}

void DummyVideoSystem::setClipboardText(zstring_view /*text*/)
{
}

std::optional<gl::ivec2> DummyVideoSystem::getWindowPosition()
{
	return {};
}

void DummyVideoSystem::setWindowPosition(gl::ivec2 /*pos*/)
{
}

void DummyVideoSystem::repaint()
{
}

} // namespace openmsx
