#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

// Modal "About Traktor" dialog. Triggered from the macOS app menu's About
// item (QAction with AboutRole). Shows the app icon, name + version, tagline,
// external links (changelog, issues, repo), and copyright/license footer.
//
// Update controls live in the menu (see MainWindow's "Check for Updates…"
// action), not in this dialog - Sparkle's progress UI cannot raise above an
// application-modal Qt panel.

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

#endif // ABOUTDIALOG_H
