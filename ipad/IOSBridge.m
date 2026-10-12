/* iPad photo export. The drawing and saving UI remains in Tux Paint. */
#import <Foundation/Foundation.h>
#import <Photos/Photos.h>
#import <UIKit/UIKit.h>
#include "SDL.h"
#include "SDL_syswm.h"
#include <math.h>

/* Keep the canvas and SDL's touch-coordinate conversion inside the safe area. */
void apple_apply_safe_area(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!window || !renderer || !SDL_GetWindowWMInfo(window, &info))
        return;

    UIView *view = info.info.uikit.window.rootViewController.view;
    CGSize bounds = view.bounds.size;
    UIEdgeInsets insets = view.safeAreaInsets;
    int width, height, logicalWidth, logicalHeight;
    if (bounds.width <= 0 || bounds.height <= 0 ||
        SDL_GetRendererOutputSize(renderer, &width, &height) != 0)
        return;
    SDL_RenderGetLogicalSize(renderer, &logicalWidth, &logicalHeight);
    if (logicalWidth <= 0 || logicalHeight <= 0)
        return;

    float pixelX = width / bounds.width;
    float pixelY = height / bounds.height;
    float left = insets.left * pixelX;
    float top = insets.top * pixelY;
    float safeWidth = width - left - insets.right * pixelX;
    float safeHeight = height - top - insets.bottom * pixelY;
    float scale = fminf(safeWidth / logicalWidth, safeHeight / logicalHeight);
    if (scale <= 0)
        return;
    SDL_Rect viewport = {
        (int)ceilf(left + (safeWidth - logicalWidth * scale) / 2),
        (int)ceilf(top + (safeHeight - logicalHeight * scale) / 2),
        (int)floorf(logicalWidth * scale),
        (int)floorf(logicalHeight * scale)
    };
    SDL_RenderSetScale(renderer, 1, 1);
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetScale(renderer, scale, scale);
}

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
