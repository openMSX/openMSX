// $Id$

#ifndef __PLATFORMFACTORY_HH__
#define __PLATFORMFACTORY_HH__

#include <string>

class Renderer;
class VDP;
class EmuTime;

/** A collection of factory methods that create platform specific objects
  * that each implement a platform independent interface.
  * The purpose of this class is to centralise the mapping from interface
  * to implementation, so that a minimum number of files have to be
  * changed when porting openMSX.
  */
class PlatformFactory
{
public:
	/** Create a Renderer.
	  * @param name the name of the Renderer to create.
	  * @param fullScreen display full screen? (rather than windowed)
	  * @param vdp the VDP whose display will be rendered.
	  * @param time the moment in emulated time the renderer is created.
	  */
	static Renderer *createRenderer(
		const std::string &name, VDP *vdp,
		bool fullScreen, const EmuTime &time);
};

#endif
