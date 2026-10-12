/* iPad photo export. The drawing and saving UI remains in Tux Paint. */
#import <Foundation/Foundation.h>
#import <Photos/Photos.h>
#import <UIKit/UIKit.h>
#include "SDL.h"
#include "SDL_syswm.h"
#include "ios.h"
#include <math.h>

static UIButton *menuButton;
static __weak UIViewController *paintController;
static SDL_Rect menuRect;

static void sendAction(int code)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = code;
    SDL_PushEvent(&event);
}

int apple_kids_mode(void)
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"TuxPaintKidsMode"];
}

static void updateMenu(void)
{
    UIAction *share = [UIAction actionWithTitle:@"Share Drawing…"
        image:[UIImage systemImageNamed:@"square.and.arrow.up"] identifier:nil
        handler:^(__unused UIAction *action) { sendAction(IOS_ACTION_SHARE); }];
    UIAction *photos = [UIAction actionWithTitle:@"Save to Photos"
        image:[UIImage systemImageNamed:@"photo"] identifier:nil
        handler:^(__unused UIAction *action) { sendAction(IOS_ACTION_PHOTOS); }];
    UIAction *kids = [UIAction actionWithTitle:@"Kids Mode"
        image:[UIImage systemImageNamed:@"hand.draw"] identifier:nil
        handler:^(__unused UIAction *action) { sendAction(IOS_ACTION_KIDS); }];
    kids.state = apple_kids_mode() ? UIMenuElementStateOn : UIMenuElementStateOff;
    UIAction *fit = [UIAction actionWithTitle:@"Fit Drawing"
        image:[UIImage systemImageNamed:@"arrow.down.right.and.arrow.up.left"] identifier:nil
        handler:^(__unused UIAction *action) { sendAction(IOS_ACTION_FIT); }];
    menuButton.menu = [UIMenu menuWithTitle:@"One finger paints. Two fingers zoom or pan."
                                  children:@[share, photos, fit, kids]];
}

void apple_set_kids_mode(int enabled)
{
    [[NSUserDefaults standardUserDefaults] setBool:enabled forKey:@"TuxPaintKidsMode"];
    updateMenu();
}

void apple_controls_enabled(int enabled)
{
    menuButton.enabled = enabled;
}

static void positionControls(SDL_Renderer *renderer)
{
    if (!menuButton) return;
    int x1, y1, x2, y2;
    SDL_RenderLogicalToWindow(renderer, menuRect.x, menuRect.y, &x1, &y1);
    SDL_RenderLogicalToWindow(renderer, menuRect.x + menuRect.w, menuRect.y + menuRect.h, &x2, &y2);
    menuButton.frame = CGRectMake(x1, y1, x2 - x1, y2 - y1);
}

void apple_install_controls(SDL_Window *window, SDL_Renderer *renderer,
                            int x, int y, int width, int height)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return;
    paintController = info.info.uikit.window.rootViewController;
    menuRect = (SDL_Rect){x, y, width, height};
    if (!menuButton) {
        menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
        [menuButton setTitle:@"Menu ▾" forState:UIControlStateNormal];
        menuButton.titleLabel.font = [UIFont boldSystemFontOfSize:18];
        menuButton.titleLabel.adjustsFontSizeToFitWidth = YES;
        menuButton.backgroundColor = [UIColor colorWithRed:0.80 green:0.94 blue:0.75 alpha:1];
        [menuButton setTitleColor:UIColor.blackColor forState:UIControlStateNormal];
        menuButton.layer.cornerRadius = 8;
        menuButton.showsMenuAsPrimaryAction = YES;
        menuButton.accessibilityLabel = @"Drawing menu";
        menuButton.accessibilityHint = @"Share, save to Photos, fit drawing, or choose Kids Mode";
        [paintController.view addSubview:menuButton];
        updateMenu();
    }
    positionControls(renderer);
}

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
    positionControls(renderer);
}

const char *apple_share_path(void)
{
    static char path[4096];
    NSString *filename = [NSTemporaryDirectory() stringByAppendingPathComponent:@"Tux Paint Drawing.png"];
    [filename getFileSystemRepresentation:path maxLength:sizeof(path)];
    return path;
}

void apple_show_share(const char *filename)
{
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!paintController || paintController.presentedViewController) return;
        UIActivityViewController *sheet = [[UIActivityViewController alloc]
            initWithActivityItems:@[url] applicationActivities:nil];
        sheet.popoverPresentationController.sourceView = menuButton;
        sheet.popoverPresentationController.sourceRect = menuButton.bounds;
        [paintController presentViewController:sheet animated:YES completion:nil];
    });
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
                else {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save to Photos",
                                                 "Your drawing was saved to Photos.", NULL);
                    });
                }
            }];
        }];
}

void apple_save_to_photos(const char *filename)
{
    exportImageToPhotos(filename);
}

void ExportPicToPhone(char *filename)
{
    exportImageToPhotos(filename);
}

void ExportGifToPhone(char *filename)
{
    exportImageToPhotos(filename);
}
