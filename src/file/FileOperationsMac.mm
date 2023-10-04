#include "FileOperationsMac.hh"

#include <Foundation/Foundation.h>
#include "FileException.hh"

namespace openmsx::FileOperations {

std::string findResourceDir(const std::string& resourceDirName)
{
	@autoreleasepool
	{
		NSBundle* mainBundle = [NSBundle mainBundle];
		if (mainBundle == nil) {
			throw FatalError("Failed to get main bundle");
		}

		NSString* resource = [NSString stringWithUTF8String:resourceDirName.c_str()];

		NSURL* resourceURL = [mainBundle URLForResource:resource withExtension:nil];
		if ([resourceURL hasDirectoryPath]) {
			return std::string(resourceURL.fileSystemRepresentation);
		}

		// Fallback when there is no application bundle or it is hidden by a symlink
		NSURL* url = [mainBundle executableURL].URLByResolvingSymlinksInPath;
		while (url != nil) {
			resourceURL = [url URLByAppendingPathComponent:[NSString stringWithFormat:@"Contents/Resources/%@", resource]];
			if ([resourceURL hasDirectoryPath]) {
				return std::string(resourceURL.fileSystemRepresentation);
			}
			resourceURL = [url URLByAppendingPathComponent:resource];
			if ([resourceURL hasDirectoryPath]) {
				return std::string(resourceURL.fileSystemRepresentation);
			}
			if (![url getResourceValue:&url forKey:NSURLParentDirectoryURLKey error:nil]) {
				url = nil;
			}
		}
	}

	throw FatalError("Could not find \"" + resourceDirName + "\" directory anywhere");
}

}
