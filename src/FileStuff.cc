// $Id$
// 
// (C) David Heremans 2002
// 
// Stuff to handle files
// Use of this library should result in the use of configurable rompaths to try to find any filenames given
// This goes for the normal file reads as well as the truncate/append file handling (cassete file for instance)

#include "FileStuff.hh"

/**
 *
 */
std::string FileStuff::findFileName(std::string filename)
{
	//TODO:Find out how in C++ to get the directory seperator
	// On Un*x like machines it is '/'
	// On M$ machines it is '\'
	// on Mac machines it is '::' if I'm not mistaken
	try
	{
	  MSXConfig::Config *config = MSXConfig::instance()->getConfigById("rompath");

	  std::string seperator =  config->getParameter("seperator");

	  std::list<const MSXConfig::Device::Parameter*> path_list;
	  path_list = config->getParametersWithClass("path");
	  std::list<const MSXConfig::Device::Parameter*>::const_iterator i;
	  for (i=path_list.begin(); i != path_list.end(); i++) {
	    std::string path =(*i)->value;
	    PRT_DEBUG("Should be testing for: " << path << seperator << filename" as file ");
	  }
	}
	catch (MSXException e)
	{
	  PRT_DEBUG("No correct rompath info in xml ?!");
	}
	
	return filename;
}


/**
 * Open a file for reading only.
 */
IFILETYPE* FileStuff::openFileRO(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " read-only ...");
	return new IFILETYPE(filename.c_str());
};
/**
 * Open a file for reading and writing.
 * if not writeable then fail
 */
IOFILETYPE* FileStuff::openFileMustRW(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " writable ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
};
/**
 * Open a file for reading and writing.
 * if not writeable then open readonly
 */
IOFILETYPE* FileStuff::openFilePreferRW(std::string filename)
{
	filename=findFileName(filename);
	IOFILETYPE* file=0;
	file=openFileMustRW(filename);
	if (file != 0){
		return file;
	};
	PRT_DEBUG("Writable failed: fallback to read-only ...");
	return openFileRO(filename);
};

/** Following are for creating/reusing files **/
/** if not writeable then fail **/
IOFILETYPE* FileStuff::openFileAppend(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " to append ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::ate);
	
};
IOFILETYPE* FileStuff::openFileTruncate(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " truncated ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::trunc);
};
