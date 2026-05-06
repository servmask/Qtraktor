#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>

// Qt-facing wrapper around Sparkle's SPUStandardUpdaterController. Hides
// Sparkle/Cocoa types from the rest of the codebase via a pimpl, so callers
// can include this header without pulling in Sparkle.h or Obj-C.
//
// Threading: Sparkle invokes its callbacks on the main thread; all signals
// emitted by this class fire on the main thread.
//
// Lifetime: instantiate once (typically owned by MainWindow). The Sparkle
// updater starts checking on construction and runs for the app's lifetime.

class UpdateManagerPrivate;

class UpdateManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canCheckForUpdates READ canCheckForUpdates NOTIFY canCheckForUpdatesChanged)
    Q_PROPERTY(bool automaticallyChecksForUpdates READ automaticallyChecksForUpdates WRITE
                   setAutomaticallyChecksForUpdates NOTIFY automaticallyChecksForUpdatesChanged)

public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override;

    // True when an update check can be initiated. Bound to Sparkle's
    // SPUUpdater.canCheckForUpdates; toggles to false while a check is
    // already in flight. UI should disable the menu item / button when false.
    bool canCheckForUpdates() const;

    // Whether Sparkle automatically checks for updates on a schedule
    // (default: on launch + every 24h). Backed by NSUserDefaults — flipping
    // it persists across app restarts.
    bool automaticallyChecksForUpdates() const;
    void setAutomaticallyChecksForUpdates(bool enabled);

public slots:
    // Triggers a user-visible update check. Sparkle shows its own progress
    // and update-available dialogs; we don't need to render UI ourselves.
    void checkForUpdates();

signals:
    void canCheckForUpdatesChanged();
    void automaticallyChecksForUpdatesChanged();

private:
    UpdateManagerPrivate *d;
};

#endif // UPDATEMANAGER_H
