// $Id$

#ifndef __FILENAMESETTING_HH__
#define __FILENAMESETTING_HH__

#include "StringSetting.hh"

namespace openmsx {

/** A Setting with a filename value.
  */
class FilenameSetting : public StringSetting
{
public:
	FilenameSetting(const string &name,
		const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual void setValue(const string &newValue);
	virtual void tabCompletion(vector<string> &tokens) const;

protected:
	/** Used by subclass to check a new file name and/or contents.
	  * The default implementation accepts any file.
	  * @param filename The file path to an existing file.
	  * @return true to accept this file name; false to reject it.
	  */
	virtual bool checkFile(const string &filename);
};

} // namespace openmsx

#endif //__FILENAMESETTING_HH__
