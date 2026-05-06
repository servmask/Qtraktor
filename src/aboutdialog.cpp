#include "aboutdialog.h"

#include <QApplication>
#include <QDate>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About Traktor"));
    setModal(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(40, 32, 40, 24);
    body->setSpacing(0);

    // App icon — pulled from the QApplication window icon set in MainWindow.
    QPixmap iconPixmap = QApplication::windowIcon().pixmap(96, 96);
    if (!iconPixmap.isNull()) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setPixmap(iconPixmap);
        iconLabel->setAlignment(Qt::AlignCenter);
        body->addWidget(iconLabel);
        body->addSpacing(20);
    }

    // Combined name + version on one line ("Traktor 1.9.1").
    auto *titleLabel = new QLabel(QStringLiteral("Traktor %1").arg(QStringLiteral(PROJECT_VERSION_STR)), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    body->addWidget(titleLabel);

    body->addSpacing(20);

    // Tagline — All-in-One WP Migration & Backup is ServMask's own plugin
    // (the one that creates .wpress files).
    auto *taglineLabel = new QLabel(tr("Extracts .wpress backup files from\n"
                                       "All-in-One WP Migration & Backup."),
                                    this);
    taglineLabel->setAlignment(Qt::AlignCenter);
    body->addWidget(taglineLabel);

    body->addSpacing(20);

    // Three external links separated by middle-dot bullets. Single rich-text
    // QLabel so the row centres as a unit and wraps cleanly on narrow widths.
    auto *linksLabel = new QLabel(this);
    linksLabel->setText(QStringLiteral("<a href=\"https://traktor.wp-migration.com/#changelog\">%1</a>"
                                       " &nbsp;·&nbsp; "
                                       "<a href=\"https://github.com/servmask/Qtraktor/issues/new/choose\">%2</a>"
                                       " &nbsp;·&nbsp; "
                                       "<a href=\"https://github.com/servmask/Qtraktor\">%3</a>")
                            .arg(tr("What's new"), tr("Report a bug"), tr("View on GitHub")));
    linksLabel->setTextFormat(Qt::RichText);
    linksLabel->setOpenExternalLinks(true);
    linksLabel->setAlignment(Qt::AlignCenter);
    linksLabel->setWordWrap(true);
    body->addWidget(linksLabel);

    outer->addLayout(body);

    // Divider above the footer.
    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    outer->addWidget(divider);

    // Footer — copyright + license, smaller and muted (palette-driven so it
    // adapts to light/dark mode).
    auto *footer = new QLabel(
        tr("© %1 ServMask Inc. · Licensed under GPLv3").arg(QString::number(QDate::currentDate().year())), this);
    QFont footerFont = footer->font();
    footerFont.setPointSize(footerFont.pointSize() - 1);
    footer->setFont(footerFont);
    footer->setAlignment(Qt::AlignCenter);
    QPalette footerPal = footer->palette();
    footerPal.setColor(QPalette::WindowText, footerPal.color(QPalette::Disabled, QPalette::WindowText));
    footer->setPalette(footerPal);
    footer->setContentsMargins(0, 12, 0, 12);
    outer->addWidget(footer);

    setFixedSize(sizeHint());
}
