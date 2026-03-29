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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        LSSetDefaultRoleHandlerForContentType(
            CFSTR("com.servmask.wpress"),
            kLSRolesAll,
            (__bridge CFStringRef)bundleId
        );

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
#pragma clang diagnostic pop
    }
}

#else

void setDockBadge(const QString &) {}
void clearDockBadge() {}
void claimFileType() {}

#endif
