// $Id$

#include "VideoSystem.hh"
#include "CommandException.hh"
#include <cassert>


namespace openmsx {

VideoSystem::~VideoSystem()
{
}

void VideoSystem::takeScreenShot(const string& /*filename*/)
{
	throw CommandException(
		"Taking screenshot not possible with current renderer." );
}

} // namespace openmsx

