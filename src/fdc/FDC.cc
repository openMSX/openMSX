// $Id$

#include "FDC.hh"
#include "DiskImageManager.hh"


FDC::FDC(MSXConfig::Device *config) 
{
	driveName[0] = config->getParameter("drivename1");
	DiskImageManager::instance()->registerDrive(driveName[0]);
	try {
		driveName[1] = config->getParameter("drivename2");
		DiskImageManager::instance()->registerDrive(driveName[1]);
		numDrives = 2;
	} catch (MSXException &e) {
		numDrives = 1;
	}
}

FDC::~FDC()
{
	for (int i = 0; i<numDrives; i++) {
		DiskImageManager::instance()->unregisterDrive(driveName[i]);
	}
}

FDCBackEnd* FDC::getBackEnd(int drive)
{
	if (drive < numDrives) {
		return DiskImageManager::instance()->
			getBackEnd(driveName[drive]);
	} else {
		throw MSXException("No such drive");
	}
}
