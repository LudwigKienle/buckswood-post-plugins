#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

#include "ReferenceFileDialog.h"

namespace buckswood_lookdna {

bool openReferenceFileDialog(
    const std::string& initialPath,
    std::string& selectedPath)
{
    __block bool accepted = false;
    __block std::string chosenPath;

    void (^showPanel)(void) = ^{
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.title = @"Choose a Look DNA reference";
            panel.prompt = @"Load Reference";
            panel.message = @"Select a JPEG, PNG, TIFF, BMP, or BWLOOK file.";
            panel.canChooseFiles = YES;
            panel.canChooseDirectories = NO;
            panel.allowsMultipleSelection = NO;
            panel.resolvesAliases = YES;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            panel.allowedFileTypes = @[
                @"jpg", @"jpeg", @"png", @"tif", @"tiff", @"bmp", @"bwlook"
            ];
#pragma clang diagnostic pop

            if (!initialPath.empty()) {
                NSString* path = [NSString stringWithUTF8String:initialPath.c_str()];
                if (path) {
                    BOOL isDirectory = NO;
                    if ([[NSFileManager defaultManager] fileExistsAtPath:path
                                                             isDirectory:&isDirectory]) {
                        NSString* directory = isDirectory
                            ? path
                            : [path stringByDeletingLastPathComponent];
                        panel.directoryURL = [NSURL fileURLWithPath:directory
                                                        isDirectory:YES];
                    }
                }
            }

            if ([panel runModal] == NSModalResponseOK && panel.URL) {
                const char* fileSystemPath = panel.URL.fileSystemRepresentation;
                if (fileSystemPath) {
                    chosenPath = fileSystemPath;
                    accepted = true;
                }
            }
        }
    };

    if ([NSThread isMainThread]) {
        showPanel();
    } else {
        dispatch_sync(dispatch_get_main_queue(), showPanel);
    }

    if (accepted) {
        selectedPath = chosenPath;
    }
    return accepted;
}

} // namespace buckswood_lookdna
