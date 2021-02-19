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
	return nullptr;
}

std::unique_ptr<V9990Rasterizer> DummyVideoSystem::createV9990Rasterizer(
	V9990& /*vdp*/)
{
	UNREACHABLE;
	return nullptr;
}

#if COMPONENT_LASERDISC
std::unique_ptr<LDRasterizer> DummyVideoSystem::createLDRasterizer(
	LaserdiscPlayer& /*ld*/)
{
	UNREACHABLE;
	return nullptr;
}
#endif

void DummyVideoSystem::flush()
{
}

gl::ivec2 DummyVideoSystem::getMouseCoord()
{
	return gl::ivec2(0, 0);
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

void DummyVideoSystem::repaint()
{
}

} // namespace openmsx
