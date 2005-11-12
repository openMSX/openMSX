// $Id$

#include "VideoSystem.hh"
#include "CommandException.hh"
#include <cassert>

namespace openmsx {

VideoSystem::~VideoSystem()
{
}

Rasterizer* VideoSystem::createRasterizer(VDP& /*vdp*/)
{
	assert(false);
	return 0;
}

bool VideoSystem::checkSettings()
{
	return true;
}

bool VideoSystem::prepare()
{
	return true;
}

void VideoSystem::takeScreenShot(const std::string& /*filename*/)
{
	throw CommandException(
		"Taking screenshot not possible with current renderer." );
}

void VideoSystem::setWindowTitle(const std::string& /*title*/)
{
	// ignore
}

} // namespace openmsx

