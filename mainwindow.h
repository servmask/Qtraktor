#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "backupfile.h"

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

  public slots:
    void openBackup();
    void extractTo();
    void extractProgress(float percent);

  private:
    Ui::MainWindow *ui;
    QString backupFilename;
};

#endif // MAINWINDOW_H
