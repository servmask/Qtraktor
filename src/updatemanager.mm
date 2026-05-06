#include "updatemanager.h"

#import <Sparkle/Sparkle.h>

// pimpl - owns the Sparkle controller. Defined in the .mm so the header can
// stay Sparkle-free.
class UpdateManagerPrivate
{
public:
    SPUStandardUpdaterController *controller = nil;
};

UpdateManager::UpdateManager(QObject *parent) : QObject(parent), d(new UpdateManagerPrivate)
{
    // startingUpdater:YES kicks off the scheduled check loop immediately.
    // Sparkle reads SUFeedURL and SUPublicEDKey from the bundle's Info.plist.
    d->controller = [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES
                                                                  updaterDelegate:nil
                                                               userDriverDelegate:nil];

    // Force automatic checks on every launch. Info.plist already sets
    // SUEnableAutomaticChecks=YES so Sparkle won't prompt the user, but we
    // also re-assert the runtime flag so a stale `defaults write` cannot
    // disable updates. There is no user-facing toggle by design.
    d->controller.updater.automaticallyChecksForUpdates = YES;
}

UpdateManager::~UpdateManager()
{
    delete d;
}

void UpdateManager::checkForUpdates()
{
    [d->controller checkForUpdates:nil];
}
