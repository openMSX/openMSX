# $Id$

SRC_HDR:= \
	File \
	FileContext \
	FileBase \
	LocalFile \
	FileOperations \
	CompressedFileAdapter \
	GZFileAdapter \
	ZipFileAdapter
	
HDR_ONLY:= \
	FileException

$(eval $(PROCESS_NODE))

