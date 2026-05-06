#include "updatemanager.h"

#import <Sparkle/Sparkle.h>

// Obj-C observer that forwards Sparkle's KVO notifications to UpdateManager.
// Holds a non-owning pointer to its Qt owner; UpdateManager removes us as a
// KVO observer in its destructor before we get released, so we never dangle.
@interface SparkleUpdateObserver : NSObject {
@public
    UpdateManager *qtOwner;
}
@end

@implementation SparkleUpdateObserver
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context
{
    Q_UNUSED(object);
    Q_UNUSED(change);
    Q_UNUSED(context);

    if ([keyPath isEqualToString:@"canCheckForUpdates"]) {
        Q_EMIT qtOwner->canCheckForUpdatesChanged();
    } else if ([keyPath isEqualToString:@"automaticallyChecksForUpdates"]) {
        Q_EMIT qtOwner->automaticallyChecksForUpdatesChanged();
    }
}
@end

// pimpl — owns the Sparkle controller and our KVO observer. Defined in the
// .mm so the header can stay Sparkle-free.
class UpdateManagerPrivate
{
public:
    SPUStandardUpdaterController *controller = nil;
    SparkleUpdateObserver *observer = nil;
};

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent), d(new UpdateManagerPrivate)
{
    // startingUpdater:YES kicks off the scheduled check loop immediately.
    // Sparkle reads SUFeedURL and SUPublicEDKey from the bundle's Info.plist.
    d->controller = [[SPUStandardUpdaterController alloc]
        initWithStartingUpdater:YES
                updaterDelegate:nil
             userDriverDelegate:nil];

    d->observer = [[SparkleUpdateObserver alloc] init];
    d->observer->qtOwner = this;

    SPUUpdater *updater = d->controller.updater;
    [updater addObserver:d->observer
              forKeyPath:@"canCheckForUpdates"
                 options:NSKeyValueObservingOptionNew
                 context:nullptr];
    [updater addObserver:d->observer
              forKeyPath:@"automaticallyChecksForUpdates"
                 options:NSKeyValueObservingOptionNew
                 context:nullptr];
}

UpdateManager::~UpdateManager()
{
    SPUUpdater *updater = d->controller.updater;
    [updater removeObserver:d->observer forKeyPath:@"canCheckForUpdates"];
    [updater removeObserver:d->observer forKeyPath:@"automaticallyChecksForUpdates"];
    delete d;
}

bool UpdateManager::canCheckForUpdates() const
{
    return d->controller.updater.canCheckForUpdates;
}

bool UpdateManager::automaticallyChecksForUpdates() const
{
    return d->controller.updater.automaticallyChecksForUpdates;
}

void UpdateManager::setAutomaticallyChecksForUpdates(bool enabled)
{
    d->controller.updater.automaticallyChecksForUpdates = enabled ? YES : NO;
}

void UpdateManager::checkForUpdates()
{
    [d->controller checkForUpdates:nil];
}
