// $Id$
// 
// Based on FileManager from David
//
// David's old comment:
// Stuff to handle files
// Use of this library should result in the use of configurable rompaths
// to try to find any filenames given
// This goes for the normal file reads as well as the truncate/append file 
// handling (cassette file for instance)

#include <stdio.h>
#include <unistd.h>

#include <libxml/nanohttp.h>
#include <libxml/nanoftp.h>

#include "FileManager.hh"

FileManager* FileManager::_instance = 0;

bool FileManager::Path::isHTTP()
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

bool FileManager::Path::isFTP()
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
		MSXConfig::CustomConfig *config = MSXConfig::Backend::instance()->getCustomConfigByTag("filepath");
	}
	catch (FileManagerException& e)
	{
		PRT_DEBUG("FileManager init exeception: " << e.desc);
	}
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
