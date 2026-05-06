#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class UpdateManager;

// Modal "About Traktor" dialog. Triggered from the macOS app menu's About
// item (QAction with AboutRole). Hosts the app icon, name, version, license
// link, and the Sparkle update controls (Check for Updates button +
// Automatically check for updates checkbox).

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    // updateManager may be nullptr — in that case the update controls are
    // disabled. Useful if we ever wire this dialog up on a platform without
    // Sparkle integration yet (Windows pending WinSparkle).
    explicit AboutDialog(UpdateManager *updateManager, QWidget *parent = nullptr);

private:
    UpdateManager *m_updateManager; // non-owning
};

#endif // ABOUTDIALOG_H
