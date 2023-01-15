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

}
