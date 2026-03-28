#include "dockprogress.h"

#ifdef Q_OS_MAC
#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>

void setDockBadge(const QString &text)
{
    @autoreleasepool {
        [[NSApp dockTile] setBadgeLabel:text.toNSString()];
    }
}

void clearDockBadge()
{
    @autoreleasepool {
        [[NSApp dockTile] setBadgeLabel:@""];
    }
}

void claimFileType()
{
    @autoreleasepool {
        NSString *bundleId = [[NSBundle mainBundle] bundleIdentifier];
        if (!bundleId) return;

        // Set Traktor as the default app for our custom UTI
        LSSetDefaultRoleHandlerForContentType(
            CFSTR("com.servmask.wpress"),
            kLSRolesAll,
            (__bridge CFStringRef)bundleId
        );

        // Also claim the .wpress extension directly via public.filename-extension
        CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(
            kUTTagClassFilenameExtension,
            CFSTR("wpress"),
            NULL
        );
        if (uti) {
            LSSetDefaultRoleHandlerForContentType(
                uti,
                kLSRolesAll,
                (__bridge CFStringRef)bundleId
            );
            CFRelease(uti);
        }
    }
}

#else

void setDockBadge(const QString &) {}
void clearDockBadge() {}
void claimFileType() {}

#endif
