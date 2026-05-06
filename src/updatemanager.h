#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>

// Qt-facing wrapper around Sparkle's SPUStandardUpdaterController. Hides
// Sparkle/Cocoa types from the rest of the codebase via a pimpl, so callers
// can include this header without pulling in Sparkle.h or Obj-C.
//
// Threading: Sparkle invokes its callbacks on the main thread.
//
// Lifetime: instantiate once (typically owned by MainWindow). The Sparkle
// updater starts its scheduled check loop on construction and runs for the
// app's lifetime. Automatic checks are forced on at every launch - there
// is no user-facing toggle by design.

class UpdateManagerPrivate;

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override;

public slots:
    // Triggers a user-visible update check. Sparkle shows its own progress
    // and update-available dialogs; we don't render any UI ourselves. The
    // call is idempotent - if a check is already in flight, Sparkle just
    // re-fronts the existing window.
    void checkForUpdates();

private:
    UpdateManagerPrivate *d;
};

#endif // UPDATEMANAGER_H
