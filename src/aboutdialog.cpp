#include "aboutdialog.h"

#include <QApplication>
#include <QDate>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

namespace
{

// Mid-luma blend of WindowText against Window - gives a muted hairline /
// secondary-text colour that adapts to light/dark mode while staying ≥4.5:1
// against Window (WCAG 2.1 AA for body text).
QColor mutedTextColor(const QPalette &pal, qreal mix = 0.55)
{
    QColor fg = pal.color(QPalette::WindowText);
    QColor bg = pal.color(QPalette::Window);
    return QColor::fromRgbF(fg.redF() * mix + bg.redF() * (1 - mix), fg.greenF() * mix + bg.greenF() * (1 - mix),
                            fg.blueF() * mix + bg.blueF() * (1 - mix));
}

// Hairline divider: 1 px filled QWidget tinted with a low-mix blend of
// WindowText into Window. Subtler than QFrame::HLine (which paints with the
// full WindowText colour and reads as a stark line on dark backgrounds).
// 3:1 against Window is the WCAG 2.1 AA threshold for non-text UI components,
// but a hairline divider is decorative - readability is not at stake - so
// we go lower (mix=0.18, ≈2:1) to match the design.
QWidget *makeHairline(QWidget *parent)
{
    auto *line = new QWidget(parent);
    line->setFixedHeight(1);
    line->setAutoFillBackground(true);
    QPalette pal = line->palette();
    pal.setColor(QPalette::Window, mutedTextColor(pal, 0.04));
    line->setPalette(pal);
    return line;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About Traktor"));
    setModal(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(60, 32, 60, 0);
    body->setSpacing(0);

    // App icon - pulled from the QApplication window icon set in MainWindow.
    QPixmap iconPixmap = QApplication::windowIcon().pixmap(65, 65);
    if (!iconPixmap.isNull()) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setPixmap(iconPixmap);
        iconLabel->setAlignment(Qt::AlignCenter);
        body->addWidget(iconLabel);
        body->addSpacing(20);
    }

    // App name - 22 px Medium (weight 500). Pixel size keeps it consistent
    // across platforms regardless of UI font DPI.
    auto *titleLabel = new QLabel(QStringLiteral("Traktor"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(22);
    titleFont.setWeight(QFont::Medium);
    titleFont.setLetterSpacing(QFont::PercentageSpacing, 105.0);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    body->addWidget(titleLabel);

    body->addSpacing(3);

    // Version line - secondary text in the same muted colour as the footer.
    auto *versionLabel = new QLabel(tr("Version %1").arg(QStringLiteral(PROJECT_VERSION_STR)), this);
    QFont versionFont = versionLabel->font();
    versionFont.setPixelSize(11);
    versionLabel->setFont(versionFont);
    versionLabel->setAlignment(Qt::AlignCenter);
    QPalette versionPal = versionLabel->palette();
    versionPal.setColor(QPalette::WindowText, mutedTextColor(versionPal, 0.55));
    versionLabel->setPalette(versionPal);
    body->addWidget(versionLabel);

    body->addSpacing(20);

    // Tagline - All-in-One WP Migration & Backup is ServMask's own plugin
    // (the one that creates .wpress files).
    auto *taglineLabel = new QLabel(tr("Extracts .wpress backup files from\n"
                                       "All-in-One WP Migration & Backup."),
                                    this);
    taglineLabel->setAlignment(Qt::AlignCenter);
    body->addWidget(taglineLabel);

    outer->addLayout(body);

    // Three external links separated by middle-dot bullets. Lives in its
    // own row with smaller horizontal margins than the body content above,
    // so all three labels + bullets fit on a single line at the dialog's
    // 380 px width. Inline text-decoration:none drops the underline
    // (mockup style); links remain identifiable by their accent colour
    // (3:1+ against body text via palette Link role) plus the pointing-hand
    // cursor on hover, satisfying WCAG 2.1 SC 1.4.1 (Use of Color).
    auto *linksLabel = new QLabel(this);
    const QString bullet = QStringLiteral(" &nbsp;&nbsp;<span style=\"color:%1\">·</span>&nbsp;&nbsp; ")
                               .arg(mutedTextColor(linksLabel->palette(), 0.16).name());
    linksLabel->setText(
        QStringLiteral("<a href=\"https://traktor.wp-migration.com/#changelog\" style=\"text-decoration:none;\">%1</a>"
                       "%4"
                       "<a href=\"https://github.com/servmask/Qtraktor/issues/new/choose\" "
                       "style=\"text-decoration:none;\">%2</a>"
                       "%4"
                       "<a href=\"https://github.com/servmask/Qtraktor\" style=\"text-decoration:none;\">%3</a>")
            .arg(tr("What's new"), tr("Report a bug"), tr("View on GitHub"), bullet));
    linksLabel->setTextFormat(Qt::RichText);
    linksLabel->setOpenExternalLinks(true);
    linksLabel->setAlignment(Qt::AlignCenter);
    linksLabel->setWordWrap(true); // graceful fallback if a translation overflows
    auto *linksRow = new QHBoxLayout;
    linksRow->setContentsMargins(16, 20, 16, 24);
    linksRow->addWidget(linksLabel);
    outer->addLayout(linksRow);

    // Hairline divider above the footer.
    outer->addWidget(makeHairline(this));

    // Footer - copyright + license, smaller and muted. Uses a 0.55 mix of
    // WindowText into Window which lands at ≥4.5:1 contrast in macOS dark
    // mode (WCAG 2.1 AA for body text). PlaceholderText would be too light
    // on some platforms; the explicit blend is portable.
    auto *footer = new QLabel(
        tr("© %1 ServMask Inc. · Licensed under GPLv3").arg(QString::number(QDate::currentDate().year())), this);
    QFont footerFont = footer->font();
    footerFont.setPointSize(footerFont.pointSize() - 1);
    footer->setFont(footerFont);
    footer->setAlignment(Qt::AlignCenter);
    QPalette footerPal = footer->palette();
    footerPal.setColor(QPalette::WindowText, mutedTextColor(footerPal, 0.55));
    footer->setPalette(footerPal);
    footer->setContentsMargins(0, 14, 0, 14);
    outer->addWidget(footer);

    // Lock width first so word-wrap / sizeHint() compute the right height,
    // then freeze the dialog at that exact size. 520 px fits all three
    // links + bullets on a single row in the system font at default size.
    setFixedWidth(380);
    adjustSize();
    setFixedSize(size());
}
