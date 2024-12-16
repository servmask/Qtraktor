#include "appdelegate.h"
#include <QFileOpenEvent>
#include <QDebug>

AppDelegate::AppDelegate(MainWindow *window, QObject *parent)
    : QObject(parent), mainWindow(window)
{
}

bool AppDelegate::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        qDebug() << "FileOpen event received at application level";
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        qDebug() << "File to open:" << openEvent->file();
        mainWindow->openBackupFile(openEvent->file());
        return true;
    }
    return QObject::eventFilter(obj, event);
} 