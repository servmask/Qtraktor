#include "dropoverlay.h"
#include <QPainter>
#include <QPen>

DropOverlay::DropOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setVisible(false);
}

void DropOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent background
    painter.fillRect(rect(), QColor(0, 0, 0, 160));

    // Dashed border
    QPen borderPen(QColor(100, 180, 255), 3, Qt::DashLine);
    borderPen.setDashPattern({8, 6});
    painter.setPen(borderPen);
    int m = 16;
    painter.drawRoundedRect(rect().adjusted(m, m, -m, -m), 12, 12);

    // Icon: draw a simple upload arrow
    QPoint center = rect().center();
    int arrowSize = 24;
    int arrowY = center.y() - 16;

    QPen iconPen(QColor(100, 180, 255), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(iconPen);

    // Arrow shaft
    painter.drawLine(center.x(), arrowY - arrowSize, center.x(), arrowY + arrowSize);
    // Arrow head
    painter.drawLine(center.x(), arrowY - arrowSize,
                     center.x() - 14, arrowY - arrowSize + 14);
    painter.drawLine(center.x(), arrowY - arrowSize,
                     center.x() + 14, arrowY - arrowSize + 14);

    // Text below arrow
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255));

    QRect textRect(0, center.y() + 28, width(), 40);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, tr("Drop .wpress file here"));
}
