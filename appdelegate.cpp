#include "appdelegate.h"
#include <QFileOpenEvent>

AppDelegate::AppDelegate(MainWindow *window, QObject *parent)
    : QObject(parent), mainWindow(window)
{
}

bool AppDelegate::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        mainWindow->openBackupFile(openEvent->file());
        return true;
    }
    return QObject::eventFilter(obj, event);
} 