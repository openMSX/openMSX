// $Id$

#include "V9990.hh"
#include "V9990Renderer.hh"
#include "Display.hh"
#include "VideoSystem.hh"

#include "openmsx.hh"

namespace openmsx {

/** Dummy Default Renderer. Any real renderer should override all
  * methods mentioned here...
  */
V9990Renderer::V9990Renderer()
{
}

V9990Renderer::~V9990Renderer()
{
}
	
void V9990Renderer::reset(const EmuTime& time)
{
}

void V9990Renderer::frameEnd(const EmuTime& time)
{
}

void V9990Renderer::renderUntil(const EmuTime& time)
{
}


} // namespace openmsx
