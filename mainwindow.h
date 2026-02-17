#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "backupfile.h"
#include "passworddialog.h"

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool event(QEvent *event) override;

  public slots:
    void openBackup();
    void openBackupFile(const QString &filename);
    void clearFile();
    void extractTo();
    void extractProgress(float percent);

  private:
    Ui::MainWindow *ui;
    QString backupFilename;
    QString filePassword;
    void showInGraphicalShell(const QString &pathIn);
};

#endif // MAINWINDOW_H
