#include "FDC_DSK.hh"
#include "MSXDiskRomPatch.hh"

FDC_DSK::FDC_DSK(MSXConfig::Device *config ) : FDCBackEnd(config)
{
  PRT_DEBUG("Creating an FDC_DSK object...");
  std::string name("diskpatch_diskA");
  try {
	std::string drive = config->getParameter("drive1");
	name[0xE]=drive[0];
	MSXConfig::Config *diskconfig = MSXConfig::Backend::instance()->getConfigById(name);
	std::string filename = diskconfig->getParameter("filename");
	std::string defaultsize = diskconfig->getParameter("defaultsize");
	disk[0] = new MSXDiskRomPatch::DiskImage(filename,defaultsize);
  } catch(MSXConfig::Exception& e) {
    PRT_DEBUG("MSXException "<< e.desc);
    PRT_INFO("Problems opening disk for drive "<<name[0xE] );
    delete disk[0];
    disk[0] = NULL;
  }
}

FDC_DSK::~FDC_DSK()
{
  PRT_DEBUG("Destroying an FDC_DSK object...");
  delete disk[0];
}

void FDC_DSK::read(byte phystrack, byte track, byte sector, byte side, int size, byte* buf)
{
  PRT_DEBUG("FDC_DSK::read(track "<<(int)track<<", sector "<<(int)sector<<", side "<<(int)side<<", size "<<(int)size<<")");
  if (disk[0]) disk[0]->readSector(buf,track*18+(sector-1)+side*9); // For double sided disks only
}
void FDC_DSK::write(byte phystrack, byte track, byte sector, byte side, int size, byte* buf)
{
  PRT_DEBUG("FDC_DSK::write(track "<<(int)track<<", sector "<<(int)sector<<", side "<<(int)side<<", size "<<(int)size<<", buf "<<std::hex<<buf<<std::dec<<")");
  if (disk[0]) disk[0]->writeSector(buf,track*18+(sector-1)+side*9);// For double sided disks only
}
