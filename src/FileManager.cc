// $Id$
// 
// Based on FileManager from David
//
// David's old comment:
// Stuff to handle files
// Use of this library should result in the use of configurable rompaths
// to try to find any filenames given
// This goes for the normal file reads as well as the truncate/append file 
// handling (cassete file for instance)

#include <stdio.h>
#include <unistd.h>

#include <libxml/nanohttp.h>
#include <libxml/nanoftp.h>

#include "FileManager.hh"

/*

<config id="filepath">
    <type>filepath</type>
    <parameter name="separator">/</parameter>
    <parameter name="1" class="path">ROMS</parameter>
    <parameter name="2" class="path">TAPES</parameter>
    <!--<parameter name="3" class="path">http://www.worldcity.nl/~andete/MSX/roms</parameter>-->
  </config>

  <config id="filecaching">
    <parameter name="cachedir">/home/andete/.msxcache</parameter>
    <parameter name="enabled">true</parameter>
    <parameter name="expire">true</parameter>
    <parameter name="expiredays">1</parameter>
  </config>

*/

std::string FileManager::findFileName(std::string filename, bool* wasURL)
{
	if (wasURL != NULL)
	{
		*wasURL = false;
	}
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
				if (FileManager::isHTTP(testfilename))
				{
					std::string localfilename(tempnam(NULL, "omsx_"));
					filename_list.push_back(localfilename);
					PRT_DEBUG("Trying to get " << testfilename << " towards " << localfilename);
					if (xmlNanoHTTPFetch(testfilename.c_str(), localfilename.c_str(), NULL)==0)
					{
						PRT_DEBUG("Found : " << testfilename << " url");
						PRT_DEBUG("Using : " << localfilename << " file");
						//IFILETYPE file(localfilename.c_str());
						filename=localfilename;
						notFound=false;
						if (wasURL != NULL)
						{
							*wasURL = true;
						}
					}

				}
				else if (FileManager::isFTP(path))
				{
					throw FileManagerException("FTP not yet implemented");
				}
				else
				{
					PRT_DEBUG("Should be testing for: " << testfilename << " as file ");
					IFILETYPE file(testfilename.c_str());
					if (!file.fail())
					{
						PRT_DEBUG("Found : " << testfilename << " file ");
						filename=testfilename;
						notFound=false;
					}
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
		PRT_DEBUG("findFileName Error: " << e.desc);
	}
	
	return filename;
}


/**
 * Open a file for reading only.
 */
IFILETYPE* FileManager::openFileRO(std::string filename)
{
	filename=findFileName(filename);
	PRT_DEBUG("Opening file " << filename << " read-only ...");
	IFILETYPE *file = new IFILETYPE(filename.c_str());
	if (file->fail()) {
		delete file;
		throw FileManagerException("Error opening file");
	}
	return file;
}

/**
 * Open a file for reading and writing.
 * if not writeable then fail
 */
IOFILETYPE* FileManager::openFileMustRW(std::string filename)
{
	bool wasURL;
	filename=findFileName(filename, &wasURL);
	if (wasURL)
	{
		throw FileManagerException("Cannot open http or ftp in read/write mode");
	}
	PRT_DEBUG("Opening file " << filename << " writable ...");
	IOFILETYPE *file = new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out);
	if (file->fail()) {
		delete file;
		throw FileManagerException("Error opening file");
	}
	return file;
}

/**
 * Open a file for reading and writing.
 * if not writeable then open readonly
 */
IOFILETYPE* FileManager::openFilePreferRW(std::string filename)
{
	bool wasURL;
	std::string full_filename=findFileName(filename, &wasURL);
	if (!wasURL)
	{
		PRT_DEBUG("Opening file " << full_filename << " writable ...");
		IOFILETYPE* file;
		file = new IOFILETYPE(full_filename.c_str(),std::ios::in|std::ios::out);
		if (file->fail()) {
			PRT_DEBUG("Writable failed: fallback to read-only ...");
			delete file;
			file = new IOFILETYPE(full_filename.c_str(),std::ios::in);
		}
		if (file->fail()) {
			delete file;
			throw FileManagerException("Error opening file");
		}
		return file;
	}
	else
	{
		PRT_DEBUG("http or ftp => using read-only ...");
		IOFILETYPE* file;
		file = new IOFILETYPE(full_filename.c_str(),std::ios::in);
		return file;
	}
}

/** Following are for creating/reusing files **/
/** if not writeable then fail **/
IOFILETYPE* FileManager::openFileAppend(std::string filename)
{
	bool wasURL;
	filename=findFileName(filename, &wasURL);
	if (wasURL)
	{
		throw FileManagerException("Cannot open http or ftp in append mode");
	}
	PRT_DEBUG("Opening file " << filename << " to append ...");
	IOFILETYPE *file = new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::ate);
	if (file->fail()) {
		delete file;
		throw FileManagerException("Error opening file");
	}
	return file; 
}

IOFILETYPE* FileManager::openFileTruncate(std::string filename)
{
	bool wasURL;
	filename=findFileName(filename, &wasURL);
	if (wasURL)
	{
		throw FileManagerException("Cannot open http or ftp in truncate mode");
	}
	PRT_DEBUG("Opening file " << filename << " truncated ...");
	IOFILETYPE *file =  new IOFILETYPE(filename.c_str(),std::ios::in|std::ios::out|std::ios::trunc);
	if (file->fail()) {
		delete file;
		throw FileManagerException("Error opening file");
	}
	return file; 
}

bool FileManager::Path::isHTTP(const std::string &path)
{
	// http://
	// 0123456
	if (path.length() < 7)
	{
		return false;
	}
	return (
		path[0]=='h' &&
		path[1]=='t' &&
		path[2]=='t' &&
		path[3]=='p' &&
		path[4]==':' &&
		path[5]=='/' &&
		path[6]=='/'
		);
}

bool FileManager::Path::isFTP(const std::string &path)
{
	// ftp://
	// 012345
	if (path.length() < 6)
	{
		return false;
	}
	return (
		path[0]=='f' &&
		path[1]=='t' &&
		path[2]=='p' &&
		path[3]==':' &&
		path[4]=='/' &&
		path[5]=='/'
		);

}

FileManager::FileManager()
{
	// init libxml protocol managers
	xmlNanoHTTPInit();
	xmlNanoFTPInit();

	// filepath
	try 
	{
		MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("filepath");
		std::list<MSXConfig::Device::Parameter*>* config_path_list = config->getParametersWithClass("path");
		std::list<MSXConfig::Device::Parameter*>::const_iterator i = config_path_list->begin();
		while (i != config_path_list->end())
		{
			path_list.push_back(new Path((*i)->value));
		}
	}
	catch (MSXConfig::Exception &e)
	{
		PRT_DEBUG("no filepath config entry in config file: " << e.desc);
	}
	
	// filecaching
	
	PRT_DEBUG("FileManager init done");
}

FileManager::~FileManager()
{
	xmlNanoHTTPCleanup();
	xmlNanoFTPCleanup();
	PRT_DEBUG("FileManager cleanup done");
}

FileManager* FileManager::instance()
{
	if (_instance==NULL)
	{
		_instance = new FileManager();
	}
	return _instance;
}

FileManager* _instance;

FileManager::Path::Path(const std::string &path_):path(path_)
{
}

FileManager::Path::~Path()
{
}
