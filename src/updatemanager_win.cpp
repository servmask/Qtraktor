#include "updatemanager.h"

#include <QApplication>
#include <QMetaObject>
#include <string>

#include <winsparkle.h>

// pimpl - retains the wstring buffers passed to win_sparkle_set_app_details.
// WinSparkle's documented contract on the lifetime of those pointers is
// opaque, so we keep them alive for the UpdateManager's full lifetime.
class UpdateManagerPrivate
{
public:
    std::wstring company;
    std::wstring appName;
    std::wstring appVersion;
};

// Free C-style callbacks for WinSparkle. WinSparkle's typedefs declare
// these as plain function pointers with no captured state.
//
// Without these, WinSparkle launches the new installer while our
// process is still running. Windows holds an exclusive lock on the
// running .exe, so the new installer cannot overwrite Traktor.exe and
// the upgrade either fails outright or evicts the running process via
// crash (the OS marks the file for deletion-on-close, and the running
// instance terminates abnormally when its own image disappears).
static int canShutdownCallback()
{
    // Always agree to shut down for an update. There is no in-flight
    // user work in Traktor that we cannot cleanly tear down.
    return 1;
}

static void shutdownRequestCallback()
{
    // Called from WinSparkle's worker thread. Marshal the quit request
    // onto the GUI thread via a queued connection so Qt's normal
    // event-loop teardown runs (QSettings sync, dtors, etc.).
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
}

UpdateManager::UpdateManager(QObject *parent) : QObject(parent), d(new UpdateManagerPrivate)
{
    d->company = L"ServMask Inc.";
    d->appName = L"Traktor";
    // PROJECT_VERSION_STR is the compile-time CMake project version, also
    // used by aboutdialog.cpp. Reading it directly avoids depending on
    // QApplication::setApplicationVersion(), which is not currently called
    // in main.cpp (so qApp->applicationVersion() returns an empty string,
    // which would break WinSparkle's version comparison against the appcast).
    d->appVersion = QStringLiteral(PROJECT_VERSION_STR).toStdWString();

    // All set_* configuration must happen before win_sparkle_init() per
    // the contract documented in winsparkle.h.
    win_sparkle_set_app_details(d->company.c_str(), d->appName.c_str(), d->appVersion.c_str());
    win_sparkle_set_appcast_url("https://github.com/servmask/Qtraktor/releases/latest/download/appcast-windows.xml");
    win_sparkle_set_eddsa_public_key("Ui6Y0zPGi/B4GEln/6fcf8wKXdHwy7BfWf2fNwE5n6c=");

    // Force automatic checks on every launch and suppress WinSparkle's
    // "may we check for updates?" first-run prompt by setting the value
    // before init(). There is no user-facing toggle by design, matching
    // the macOS Sparkle integration.
    win_sparkle_set_automatic_check_for_updates(1);

    // Tell WinSparkle we can be asked to quit before an installer runs
    // and how to do it. Without these, the running Traktor.exe holds
    // an exclusive lock on its own image and the upgrade either fails
    // or evicts the running process abnormally.
    win_sparkle_set_can_shutdown_callback(canShutdownCallback);
    win_sparkle_set_shutdown_request_callback(shutdownRequestCallback);

    win_sparkle_init();
}

UpdateManager::~UpdateManager()
{
    win_sparkle_cleanup();
    delete d;
}

void UpdateManager::checkForUpdates()
{
    win_sparkle_check_update_with_ui();
}
