// $Id$

#ifndef __VIDEOSYSTEM_HH__
#define __VIDEOSYSTEM_HH__

#include <string>

using std::string;


namespace openmsx {

/** Video back-end system.
  */
class VideoSystem
{
public:
	virtual ~VideoSystem();

	/** Finish pending drawing operations and make them visible to the user.
	  */
	virtual void flush() = 0;

	/** Take a screenshot.
	  * @param filename Name of the file to save the screenshot to.
	  */
	virtual void takeScreenShot(const string& filename);
};

} // namespace openmsx

#endif //__VIDEOSYSTEM_HH__

