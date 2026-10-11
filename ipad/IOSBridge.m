/* iPad photo export. The drawing and saving UI remains in Tux Paint. */
#import <Foundation/Foundation.h>
#import <Photos/Photos.h>
#include "SDL.h"

static void showExportError(NSString *message)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Save to Photos",
                                 [message UTF8String], NULL);
    });
}

static void exportImageToPhotos(const char *filename)
{
    NSString *path = [NSString stringWithUTF8String:filename];
    if (path == nil) {
        showExportError(@"The picture's filename could not be read.");
        return;
    }
    NSURL *url = [NSURL fileURLWithPath:path];

    [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelAddOnly
        handler:^(PHAuthorizationStatus status) {
            if (status != PHAuthorizationStatusAuthorized &&
                status != PHAuthorizationStatusLimited) {
                showExportError(@"Your picture is still saved in Tux Paint. Allow Tux Paint to add photos in Settings to also save it to Photos.");
                return;
            }
            [[PHPhotoLibrary sharedPhotoLibrary] performChanges:^{
                [PHAssetChangeRequest creationRequestForAssetFromImageAtFileURL:url];
            } completionHandler:^(BOOL success, NSError *error) {
                if (!success) {
                    showExportError(error.localizedDescription ?: @"The picture could not be added to Photos. Your saved picture is still in Tux Paint.");
                }
            }];
        }];
}

void ExportPicToPhone(char *filename)
{
    exportImageToPhotos(filename);
}

void ExportGifToPhone(char *filename)
{
    exportImageToPhotos(filename);
}
