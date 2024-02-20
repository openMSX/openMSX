#include "VideoSystem.hh"
#include "MSXException.hh"

namespace openmsx {

void VideoSystem::takeScreenShot(
	const std::string& /*filename*/, bool /*withOsd*/)
{
	throw MSXException(
		"Taking screenshot not possible with current renderer.");
}

void VideoSystem::updateWindowTitle()
{
	// ignore
}

} // namespace openmsx
