#include "aboutdialog.h"

#include <QApplication>
#include <QDate>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#ifdef Q_OS_MAC
#include "updatemanager.h"
#include <QCheckBox>
#include <QPushButton>
#include <QSignalBlocker>
#endif

AboutDialog::AboutDialog(UpdateManager *updateManager, QWidget *parent)
    : QDialog(parent), m_updateManager(updateManager)
{
    setWindowTitle(tr("About Traktor"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 20);
    layout->setSpacing(6);

    // App icon — only rendered if QApplication has a window icon set.
    QPixmap iconPixmap = QApplication::windowIcon().pixmap(96, 96);
    if (!iconPixmap.isNull()) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setPixmap(iconPixmap);
        iconLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(iconLabel);
        layout->addSpacing(8);
    }

    // App name
    auto *nameLabel = new QLabel(QStringLiteral("Traktor"), this);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 6);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    // Version
    auto *versionLabel = new QLabel(tr("Version %1").arg(QStringLiteral(PROJECT_VERSION_STR)), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    layout->addSpacing(10);

    // Tagline — All-in-One WP Migration & Backup is ServMask's own plugin
    // (the one that creates .wpress files); link points to its WP.org page.
    auto *taglineLabel = new QLabel(tr("Extracts .wpress backup files from<br>"
                                       "<a href=\"https://wordpress.org/plugins/all-in-one-wp-migration/\">"
                                       "All-in-One WP Migration &amp; Backup</a>."),
                                    this);
    taglineLabel->setAlignment(Qt::AlignCenter);
    taglineLabel->setOpenExternalLinks(true);
    taglineLabel->setTextFormat(Qt::RichText);
    layout->addWidget(taglineLabel);

    layout->addSpacing(10);

    // Homepage + license links
    auto *linksLabel =
        new QLabel(tr("<a href=\"https://github.com/servmask/Qtraktor\">github.com/servmask/Qtraktor</a><br>"
                      "Licensed under <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GPLv3</a>"),
                   this);
    linksLabel->setAlignment(Qt::AlignCenter);
    linksLabel->setOpenExternalLinks(true);
    layout->addWidget(linksLabel);

    layout->addSpacing(10);

    // Copyright — matches the canonical format used in ServMask's PHP
    // codebases ("Copyright (C) 2014-YYYY ServMask Inc."). Year auto-updates
    // via QDate::currentDate() so we never have to remember to bump it.
    auto *copyrightLabel =
        new QLabel(tr("Copyright © 2014-%1 ServMask Inc.").arg(QString::number(QDate::currentDate().year())), this);
    QFont copyrightFont = copyrightLabel->font();
    copyrightFont.setPointSize(copyrightFont.pointSize() - 1);
    copyrightLabel->setFont(copyrightFont);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

#ifdef Q_OS_MAC
    // Update controls — only meaningful on macOS where Sparkle is integrated.
    // Windows/Linux users see the dialog without these widgets at all.
    if (m_updateManager) {
        layout->addSpacing(16);

        // Check for Updates button — bound to UpdateManager::canCheckForUpdates
        auto *checkButton = new QPushButton(tr("Check for Updates..."), this);
        layout->addWidget(checkButton, 0, Qt::AlignCenter);
        checkButton->setEnabled(m_updateManager->canCheckForUpdates());
        connect(checkButton, &QPushButton::clicked, m_updateManager, &UpdateManager::checkForUpdates);
        connect(m_updateManager, &UpdateManager::canCheckForUpdatesChanged, this,
                [this, checkButton] { checkButton->setEnabled(m_updateManager->canCheckForUpdates()); });

        layout->addSpacing(10);

        // Auto-check checkbox — two-way bound to
        // UpdateManager::automaticallyChecksForUpdates (NSUserDefaults-backed,
        // persists across launches via Sparkle).
        auto *autoCheckBox = new QCheckBox(tr("Automatically check for updates"), this);
        layout->addWidget(autoCheckBox, 0, Qt::AlignCenter);
        autoCheckBox->setChecked(m_updateManager->automaticallyChecksForUpdates());
        connect(autoCheckBox, &QCheckBox::toggled, m_updateManager, &UpdateManager::setAutomaticallyChecksForUpdates);
        // Reflect external changes (e.g. another window of the app, or
        // Sparkle's own UI flipping the value) without re-emitting toggled().
        connect(m_updateManager, &UpdateManager::automaticallyChecksForUpdatesChanged, this, [this, autoCheckBox] {
            QSignalBlocker blocker(autoCheckBox);
            autoCheckBox->setChecked(m_updateManager->automaticallyChecksForUpdates());
        });
    }
#else
    Q_UNUSED(m_updateManager);
#endif

    layout->addSpacing(12);

    // Close button
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    setFixedSize(sizeHint());
}
