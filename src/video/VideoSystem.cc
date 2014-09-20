#include "VideoSystem.hh"
#include "MSXException.hh"

namespace openmsx {

bool VideoSystem::checkSettings()
{
	return true;
}

void VideoSystem::takeScreenShot(
	const std::string& /*filename*/, bool /*withOsd*/)
{
	throw MSXException(
		"Taking screenshot not possible with current renderer.");
}

void VideoSystem::setWindowTitle(const std::string& /*title*/)
{
	// ignore
}

} // namespace openmsx
