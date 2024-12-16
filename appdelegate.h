#ifndef APPDELEGATE_H
#define APPDELEGATE_H

#include <QObject>
#include "mainwindow.h"

class AppDelegate : public QObject
{
    Q_OBJECT
public:
    explicit AppDelegate(MainWindow *window, QObject *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    MainWindow *mainWindow;
};

#endif // APPDELEGATE_H 