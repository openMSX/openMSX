#include "FileOperationsMac.hh"

#include <Foundation/Foundation.h>
#include "FileException.hh"

namespace openmsx::FileOperations {

std::string findShareDir()
{
	@autoreleasepool
	{
		NSBundle* mainBundle = [NSBundle mainBundle];
		if (mainBundle == nil) {
			throw FatalError("Failed to get main bundle");
		}

		NSURL* shareURL = [mainBundle URLForResource:@"share" withExtension:nil];
		if ([shareURL hasDirectoryPath]) {
			return std::string(shareURL.fileSystemRepresentation);
		}

		// Fallback when there is no application bundle or it is hidden by a symlink
		NSURL* url = [mainBundle executableURL].URLByResolvingSymlinksInPath;
		while (url != nil) {
			shareURL = [url URLByAppendingPathComponent:@"Contents/Resources/share"];
			if ([shareURL hasDirectoryPath]) {
				return std::string(shareURL.fileSystemRepresentation);
			}
			shareURL = [url URLByAppendingPathComponent:@"share"];
			if ([shareURL hasDirectoryPath]) {
				return std::string(shareURL.fileSystemRepresentation);
			}
			if (![url getResourceValue:&url forKey:NSURLParentDirectoryURLKey error:nil]) {
				url = nil;
			}
		}
	}

	throw FatalError("Could not find \"share\" directory anywhere");
}


// TODO: remove duplication, but I don't know how to program this properly
std::string findDocDir()
{
	@autoreleasepool
	{
		NSBundle* mainBundle = [NSBundle mainBundle];
		if (mainBundle == nil) {
			throw FatalError("Failed to get main bundle");
		}

		NSURL* docURL = [mainBundle URLForResource:@"doc" withExtension:nil];
		if ([docURL hasDirectoryPath]) {
			return std::string(docURL.fileSystemRepresentation);
		}

		// Fallback when there is no application bundle or it is hidden by a symlink
		NSURL* url = [mainBundle executableURL].URLByResolvingSymlinksInPath;
		while (url != nil) {
			docURL = [url URLByAppendingPathComponent:@"Contents/Resources/doc"];
			if ([docURL hasDirectoryPath]) {
				return std::string(docURL.fileSystemRepresentation);
			}
			docURL = [url URLByAppendingPathComponent:@"doc"];
			if ([docURL hasDirectoryPath]) {
				return std::string(docURL.fileSystemRepresentation);
			}
			if (![url getResourceValue:&url forKey:NSURLParentDirectoryURLKey error:nil]) {
				url = nil;
			}
		}
	}

	throw FatalError("Could not find \"doc\" directory anywhere");
}

}
