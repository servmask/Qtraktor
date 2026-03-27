#include "progresswindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QFont>
#include <QStyle>

ProgressWindow::ProgressWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("Traktor"));
    setFixedWidth(420);

    // App icon on the left
    m_iconLabel = new QLabel;
    QPixmap appIcon = QApplication::windowIcon().pixmap(32, 32);
    if (appIcon.isNull()) {
        appIcon = style()->standardPixmap(QStyle::SP_FileIcon);
    }
    m_iconLabel->setPixmap(appIcon);
    m_iconLabel->setFixedSize(36, 36);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    // "Extracting "filename.wpress"" label - single line, elided
    m_fileLabel = new QLabel;
    m_fileLabel->setTextFormat(Qt::PlainText);

    // Progress bar - native, compact
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(16);

    // Cancel button (small X)
    m_cancelButton = new QPushButton;
    m_cancelButton->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    m_cancelButton->setFixedSize(24, 24);
    m_cancelButton->setFlat(true);
    m_cancelButton->setToolTip(tr("Stop"));
    connect(m_cancelButton, &QPushButton::clicked, this, &ProgressWindow::cancelClicked);

    // Phase label below (e.g. time estimate or status)
    m_phaseLabel = new QLabel;
    QFont smallFont = m_phaseLabel->font();
    smallFont.setPointSize(smallFont.pointSize() - 2);
    m_phaseLabel->setFont(smallFont);
    QPalette pal = m_phaseLabel->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
    m_phaseLabel->setPalette(pal);

    // Row 1: icon | file label + progress bar + cancel
    QHBoxLayout *progressRow = new QHBoxLayout;
    progressRow->setContentsMargins(0, 0, 0, 0);
    progressRow->setSpacing(6);
    progressRow->addWidget(m_progressBar, 1);
    progressRow->addWidget(m_cancelButton);

    // Right side: file label on top, progress row below
    QVBoxLayout *rightSide = new QVBoxLayout;
    rightSide->setContentsMargins(0, 0, 0, 0);
    rightSide->setSpacing(4);
    rightSide->addWidget(m_fileLabel);
    rightSide->addLayout(progressRow);
    rightSide->addWidget(m_phaseLabel);

    // Main row: icon + right side
    QHBoxLayout *mainRow = new QHBoxLayout;
    mainRow->setContentsMargins(12, 10, 12, 10);
    mainRow->setSpacing(10);
    mainRow->addWidget(m_iconLabel, 0, Qt::AlignTop);
    mainRow->addLayout(rightSide, 1);

    setLayout(mainRow);

    // Center horizontally, upper third of screen
    if (QScreen *screen = QApplication::primaryScreen()) {
        QRect geo = screen->availableGeometry();
        move(geo.center().x() - width() / 2, geo.top() + geo.height() / 4);
    }
}

void ProgressWindow::setFileName(const QString &name)
{
    // Elide long filenames with "..." in the middle, like Keka
    QFontMetrics fm(m_fileLabel->font());
    int availWidth = width() - 90; // icon + margins + cancel button
    QString elided = fm.elidedText(name, Qt::ElideMiddle, availWidth);
    m_fileLabel->setText(tr("Extracting \"%1\"").arg(elided));
}

void ProgressWindow::setPhase(const QString &phase)
{
    m_phaseLabel->setText(phase);
}

void ProgressWindow::setProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void ProgressWindow::setQueueInfo(int remaining)
{
    if (remaining > 0) {
        setWindowTitle(tr("Traktor (%1 more)").arg(remaining));
    } else {
        setWindowTitle(tr("Traktor"));
    }
}
