#include "dropoverlay.h"
#include "clouddownloader.h"
#include <QPainter>
#include <QPen>
#include <QDragEnterEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QUrl>

DropOverlay::DropOverlay(QWidget *parent) : QWidget(parent)
{
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void DropOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

void DropOverlay::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void DropOverlay::setFileName(const QString &name)
{
    m_fileName = name;
    update();
}

// Returns a usable QUrl from the first line of a plain-text string, or an
// invalid QUrl if the text doesn't look like a remote URL. Used as a fallback
// when text/uri-list is absent or strips the URL fragment (e.g. Mega #KEY).
static QUrl urlFromPlainText(const QString &text)
{
    const QString line = text.trimmed().split(QLatin1Char('\n')).first().trimmed();
    const QUrl url = QUrl::fromUserInput(line);
    return CloudDownloader::isRemoteUrl(url) ? url : QUrl();
}

void DropOverlay::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();

    // ── Primary path: text/uri-list ───────────────────────────────────────
    // Covers local-file drags and most remote URL drags.
    // NOTE: text/uri-list strips URL fragments (RFC 2483), so Mega links
    // dragged via this path will lose their #KEY — the text/plain fallback
    // below catches that case with the fragment intact.
    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        if (urls.count() == 1) {
            const QUrl &url = urls.first();
            const QString localFile = url.toLocalFile();
            if ((!localFile.isEmpty() && localFile.endsWith(QLatin1String(".wpress"), Qt::CaseInsensitive)) ||
                CloudDownloader::isRemoteUrl(url)) {
                setHighlighted(true);
                event->acceptProposedAction();
                return;
            }
        }
    }

    // ── Fallback: text/plain containing a URL ─────────────────────────────
    // Cloud storage web UIs (Google Drive, Dropbox, Mega, …) often do not
    // populate text/uri-list at all; they put the URL in text/plain instead.
    // text/plain also preserves URL fragments, which is essential for Mega
    // links where the decryption key lives after the #.
    if (mime->hasText()) {
        if (urlFromPlainText(mime->text()).isValid()) {
            setHighlighted(true);
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void DropOverlay::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    setHighlighted(false);
}

void DropOverlay::dropEvent(QDropEvent *event)
{
    setHighlighted(false);
    const QMimeData *mime = event->mimeData();

    // ── Primary path: text/uri-list ───────────────────────────────────────
    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        if (urls.count() == 1) {
            const QUrl &url = urls.first();
            const QString localFile = url.toLocalFile();
            if (!localFile.isEmpty() && localFile.endsWith(QLatin1String(".wpress"), Qt::CaseInsensitive)) {
                emit fileDropped(localFile);
                event->acceptProposedAction();
                return;
            }
            if (CloudDownloader::isRemoteUrl(url)) {
                emit urlDropped(url);
                event->acceptProposedAction();
                return;
            }
        }
    }

    // ── Fallback: text/plain containing a URL ─────────────────────────────
    // Handles cloud web-UI drags and preserves Mega #KEY fragments.
    if (mime->hasText()) {
        const QUrl url = urlFromPlainText(mime->text());
        if (url.isValid()) {
            emit urlDropped(url);
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void DropOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool hasFile = !m_fileName.isEmpty();

    // Background: gray when empty, blue tint when file loaded
    QColor bgColor;
    QColor borderColor;
    if (m_highlighted) {
        bgColor = QColor(40, 80, 130, 80);
        borderColor = QColor(80, 160, 240);
    } else if (hasFile) {
        bgColor = QColor(30, 60, 110, 50);
        borderColor = QColor(80, 160, 240);
    } else {
        bgColor = QColor(120, 120, 120, 40);
        borderColor = QColor(160, 160, 160);
    }
    painter.fillRect(rect(), bgColor);

    // Dashed border
    QPen borderPen(borderColor, 2, Qt::DashLine);
    borderPen.setDashPattern({8, 5});
    painter.setPen(borderPen);
    int m = 10;
    painter.drawRoundedRect(rect().adjusted(m, m, -m, -m), 10, 10);

    QPoint center = rect().center();
    QRect inner = rect().adjusted(m + 16, m + 8, -(m + 16), -(m + 8));

    if (hasFile) {
        // File loaded state: show filename with word wrap
        QFont font = painter.font();
        font.setPointSize(11);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor(80, 160, 240));
        painter.drawText(inner, Qt::AlignCenter | Qt::TextWordWrap, m_fileName);
    } else {
        // Empty state: show upload icon + text
        int arrowSize = 20;
        int arrowY = center.y() - 14;

        QPen iconPen(borderColor, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(iconPen);

        // Arrow shaft
        painter.drawLine(center.x(), arrowY - arrowSize, center.x(), arrowY + arrowSize);
        // Arrow head
        painter.drawLine(center.x(), arrowY - arrowSize, center.x() - 12, arrowY - arrowSize + 12);
        painter.drawLine(center.x(), arrowY - arrowSize, center.x() + 12, arrowY - arrowSize + 12);

        // Text
        QFont font = painter.font();
        font.setPointSize(11);
        painter.setFont(font);
        painter.setPen(borderColor);
        QRect textRect(0, center.y() + 22, width(), 30);
        painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, tr("Drop .wpress file here"));
    }
}
