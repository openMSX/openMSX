# $Id$

include build/node-start.mk

SRC_HDR:= \
	File \
	FileContext \
	Filename \
	FileBase \
	LocalFile \
	FileOperations \
	CompressedFileAdapter \
	GZFileAdapter \
	ZipFileAdapter \
	ZlibInflate \
	ReadDir \
	FilePool \
	PreCacheFile \
	LocalFileReference

HDR_ONLY:= \
	FileException

include build/node-end.mk

