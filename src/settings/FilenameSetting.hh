// $Id$

#ifndef __FILENAMESETTING_HH__
#define __FILENAMESETTING_HH__

#include "StringSetting.hh"
#include "NonInheritable.hh"

namespace openmsx {

/** A Setting with a filename value.
  */
class FilenameSettingBase : public StringSettingBase
{
public:
	// Implementation of Setting interface:
	virtual void setValue(const string& newValue);
	virtual void tabCompletion(vector<string>& tokens) const;

protected:
	FilenameSettingBase(const string& name, const string& description,
	                    const string& initialValue, XMLElement* node);

	/** Used by subclass to check a new file name and/or contents.
	  * @param filename The file path to an existing file.
	  * @return true to accept this file name; false to reject it.
	  */
	virtual bool checkFile(const string& filename) = 0;
};

NON_INHERITABLE_PRE(FilenameSetting)
class FilenameSetting : public FilenameSettingBase,
                        NON_INHERITABLE(FilenameSetting)
{
public:
	FilenameSetting(const string& name, const string& description,
	                const string& initialValue, XMLElement* node = NULL);
	virtual ~FilenameSetting();
	
	virtual bool checkFile(const string& filename);
};

} // namespace openmsx

#endif //__FILENAMESETTING_HH__
