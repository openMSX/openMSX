// $Id$

#include "VideoSystem.hh"
#include "CommandException.hh"

namespace openmsx {

VideoSystem::~VideoSystem()
{
}

bool VideoSystem::checkSettings()
{
	return true;
}

void VideoSystem::takeScreenShot(
	const std::string& /*filename*/, bool /*withOsd*/)
{
	throw CommandException(
		"Taking screenshot not possible with current renderer.");
}

void VideoSystem::setWindowTitle(const std::string& /*title*/)
{
	// ignore
}

} // namespace openmsx
