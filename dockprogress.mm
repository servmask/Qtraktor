#include "dockprogress.h"

#ifdef Q_OS_MAC
#import <AppKit/AppKit.h>

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

#else

void setDockBadge(const QString &) {}
void clearDockBadge() {}

#endif
