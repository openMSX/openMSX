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

		// Fallback when there is no application bundle
		NSURL* url = [mainBundle executableURL].URLByResolvingSymlinksInPath;
		while (url != nil) {
			shareURL = [url URLByAppendingPathComponent:@"share" isDirectory:YES];
			NSNumber* isDirectory;
			BOOL success = [shareURL getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
			if (success && [isDirectory boolValue]) {
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
