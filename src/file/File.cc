// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "MSXConfig.hh"

#ifdef __WIN32__
#define SEPARATOR '\\'
#else
#define SEPARATOR '/'
#endif


File::File(const std::string &url, FileType fileType, int options)
{
	std::string newURL;
	file = searchFile(url, newURL, fileType, options);
}

const std::string File::findName(const std::string &url, FileType fileType,
                                 int options)
{
	std::string newURL;
	FileBase* file = searchFile(url, newURL, fileType, options);
	delete file;
	return newURL;
}

FileBase* File::searchFile(const std::string &url, std::string &newURL,
                           FileType fileType, int options)
{
	std::string type;
	switch (fileType) {
		case DISK:
			type = "disk";
			break;
		case ROM:
			type = "rom";
			break;
		case TAPE:
			type = "tape";
			break;
		case STATE:
			type = "state";
			break;
		case CONFIG:
			type = "config";
			break;
		default:
			// do nothing
			break;
	}
	
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->
			getConfigById("SearchPaths");
		std::list<MSXConfig::Device::Parameter*>* pathList;
		pathList = config->getParametersWithClass(type);
		std::list<MSXConfig::Device::Parameter*>::const_iterator it;
		for (it = pathList->begin(); it != pathList->end(); it++) {
			std::string path = (*it)->value;
			newURL = path + SEPARATOR + url;
			try {
				return openFile(newURL, options);
			} catch (FileException &e) {
				// try next
			}
		}
	} catch (MSXException &e) {
		// no SearchPath section in config
	}

	// if everything fails, try original name
	newURL = url;
	return openFile(url, options);
}

FileBase* File::openFile(const std::string &url, int options)
{
	// TODO for the moment only support for LocalFile
	PRT_DEBUG("File: " << url);
	return new LocalFile(url, options);
}


File::~File()
{
	delete file;
}

void File::read(byte* buffer, int num)
{
	file->read(buffer, num);
}

void File::write(const byte* buffer, int num)
{
	file->write(buffer, num);
}

byte* File::mmap(bool write)
{
	return file->mmap(write);
}

void File::munmap()
{
	file->munmap();
}

int File::size()
{
	return file->size();
}

void File::seek(int pos)
{
	file->seek(pos);
}

int File::pos()
{
	return file->pos();
}
