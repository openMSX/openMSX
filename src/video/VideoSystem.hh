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

	/** Prepare video system for drawing operations.
	  * The default implementation does nothing, successfully.
	  * @return true iff successful.
	  * TODO: What should be done if preparation is not successful?
	  */
	virtual bool prepare();

	/** Finish pending drawing operations and make them visible to the user.
	  */
	virtual void flush() = 0;

	/** Take a screenshot.
	  * The default implementation throws an exception.
	  * @param filename Name of the file to save the screenshot to.
	  * @throw CommandException If taking the screen shot fails.
	  */
	virtual void takeScreenShot(const string& filename);
};

} // namespace openmsx

#endif //__VIDEOSYSTEM_HH__

