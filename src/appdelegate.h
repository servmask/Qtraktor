#ifndef APPDELEGATE_H
#define APPDELEGATE_H

#include <QObject>

class MainWindow;
class AutoExtractor;

class AppDelegate : public QObject
{
    Q_OBJECT
public:
    explicit AppDelegate(MainWindow *window, AutoExtractor *extractor = nullptr, QObject *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    MainWindow *mainWindow;
    AutoExtractor *autoExtractor;
};

#endif // APPDELEGATE_H
