# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXFDC \
	WD2793BasedFDC \
	PhilipsFDC \
	NationalFDC \
	MicrosolFDC \
	WD2793 \
	TurboRFDC \
	TC8566AF \
	DiskImageCLI \
	DiskDrive \
	DiskChanger \
	DriveMultiplexer \
	Disk \
	SectorBasedDisk \
	DummyDisk \
	DSKDiskImage \
	XSADiskImage \
	CRC16 \
	FDC_DirAsDSK \
	EmptyDiskPatch \
	RamDSKDiskImage \
	MSXtar \
	DiskContainer \
	SectorAccessibleDisk \
	FileManipulator \
	BootBlocks \

include build/node-end.mk

