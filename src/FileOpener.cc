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
		MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("rompath");

		std::string separator =  config->getParameter("separator");
		if (strstr(filename.c_str(),separator.c_str())==0)
		{
			std::list<MSXConfig::Device::Parameter*>* path_list;
			path_list = config->getParametersWithClass("path");
			std::list<MSXConfig::Device::Parameter*>::const_iterator i;
			bool notFound=true;
			for (i=path_list->begin(); (i != path_list->end()) && notFound ; i++)
			{
				std::string path =(*i)->value;
				std::string testfilename=path + separator + filename;
				PRT_DEBUG("Should be testing for: " << testfilename << " as file ");
				IFILETYPE file(testfilename.c_str());
				if (!file.fail())
				{
					PRT_DEBUG("Found : " << testfilename << " file ");
					filename= testfilename;
					notFound=false;
				}
			}
			config->getParametersWithClassClean(path_list);
		}
		else
		{
			PRT_DEBUG("Directory-separator found in filename ");
		}
	}
	catch (MSXException& e)
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
	IFILETYPE *file = new IFILETYPE(filename.c_str());
	if (file->fail()) throw FileOpenerException("Error opening file");
	return file;
}

/**
 * Open a file for reading and writing.
 * if not writeable then fail
 */
IOFILETYPE* FileOpener::openFileMustRW(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " writable ...");
	IOFILETYPE *file = new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
	if (file->fail()) throw FileOpenerException("Error opening file");
	return file; 
}

/**
 * Open a file for reading and writing.
 * if not writeable then open readonly
 */
IOFILETYPE* FileOpener::openFilePreferRW(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " writable ...");
	IOFILETYPE* file;
	file = new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
	if (file->fail()) {
		PRT_DEBUG("Writable failed: fallback to read-only ...");
		delete file;
		file = new IOFILETYPE(filename.c_str(),std::ios::in);
	}
	if (file->fail()) throw FileOpenerException("Error opening file");
	return file; 
}

/** Following are for creating/reusing files **/
/** if not writeable then fail **/
IOFILETYPE* FileOpener::openFileAppend(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " to append ...");
	IOFILETYPE *file = new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::ate);
	if (file->fail()) throw FileOpenerException("Error opening file");
	return file; 
}

IOFILETYPE* FileOpener::openFileTruncate(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " truncated ...");
	IOFILETYPE *file =  new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::trunc);
	if (file->fail()) throw FileOpenerException("Error opening file");
	return file; 
}
