// $Id$
// 
// (C) David Heremans 2002
// 
// Stuff to handle files
// Use of this library should result in the use of configurable rompaths
// to try to find any filenames given
// This goes for the normal file reads as well as the truncate/append file 
// handling (cassete file for instance)

#include "FileOpener.hh"

/**
 *
 */
std::string FileOpener::findFileName(std::string filename)
{
	//TODO:Find out how in C++ to get the directory separator
	// On Un*x like machines it is '/'
	// On M$ machines it is '\'
	// on Mac machines it is ':'
	try
	{
	  MSXConfig::Config *config = MSXConfig::instance()->getConfigById("rompath");

	  std::string separator =  config->getParameter("separator");
	  if (strstr(filename.c_str(),separator.c_str())==0){
	    std::list<const MSXConfig::Device::Parameter*> path_list;
	    path_list = config->getParametersWithClass("path");
	    std::list<const MSXConfig::Device::Parameter*>::const_iterator i;
	    bool notFound=true;
	    for (i=path_list.begin(); (i != path_list.end()) && notFound ; i++) {
	      std::string path =(*i)->value;
	      std::string testfilename=path + separator + filename;
	      PRT_DEBUG("Should be testing for: " << testfilename << " as file ");
	      IFILETYPE *file=new IFILETYPE(testfilename.c_str());
	      if (!file->fail()){
	        PRT_DEBUG("Found : " << testfilename << " file ");
	      	filename= testfilename;
		notFound=false;
	      }
	    }
	  } else {
	      PRT_DEBUG("Directory-separator found in filename ");
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
IFILETYPE* FileOpener::openFileRO(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " read-only ...");
	return new IFILETYPE(filename.c_str());
};
/**
 * Open a file for reading and writing.
 * if not writeable then fail
 */
IOFILETYPE* FileOpener::openFileMustRW(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " writable ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
};
/**
 * Open a file for reading and writing.
 * if not writeable then open readonly
 */
IOFILETYPE* FileOpener::openFilePreferRW(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " writable ...");
	IOFILETYPE* file=new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
	if (!file->fail()){
		return file;
	};
	PRT_DEBUG("Writable failed: fallback to read-only ...");
	delete file;
	filename=findFileName(filename);
	return new IOFILETYPE(filename.c_str(),std::ios::in);
};

/** Following are for creating/reusing files **/
/** if not writeable then fail **/
IOFILETYPE* FileOpener::openFileAppend(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " to append ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::ate);
	
};
IOFILETYPE* FileOpener::openFileTruncate(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " truncated ...");
	return new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::trunc);
};
