// $Id$

#ifndef __PLATFORMFACTORY_HH__
#define __PLATFORMFACTORY_HH__

namespace openmsx {

/** A collection of factory methods that create platform specific objects
  * that each implement a platform independent interface.
  * The purpose of this class is to centralise the mapping from interface
  * to implementation, so that a minimum number of files have to be
  * changed when porting openMSX.
  */
class PlatformFactory
{
	// TODO: Because currently everything is built on SDL,
	//       that takes care of porting issues.
	//       I (mthuurne) think in the future this class will be useful.
	//       If I am wrong, this class should be removed.
};

} // namespace openmsx

#endif
