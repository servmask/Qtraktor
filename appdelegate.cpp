#include "appdelegate.h"
#include "mainwindow.h"
#include "autoextractor.h"
#include <QFileOpenEvent>

AppDelegate::AppDelegate(MainWindow *window, AutoExtractor *extractor, QObject *parent)
    : QObject(parent), mainWindow(window), autoExtractor(extractor)
{
}

bool AppDelegate::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        const QString file = openEvent->file();

        if (autoExtractor) {
            autoExtractor->enqueueFile(file);
        } else if (mainWindow) {
            mainWindow->openBackupFile(file);
        }
        return true;
    }
    return QObject::eventFilter(obj, event);
}
