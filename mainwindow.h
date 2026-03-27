#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "backupfile.h"
#include "extractionworker.h"
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
  public slots:
    void openBackup();
    void openBackupFile(const QString &filename);
    void setPassword(const QString &password);
    void clearFile();
    void extractTo();
    void extractToPath(const QString &destDir);
    void extractProgress(float percent);

  private slots:
    void onExtractionFinished(bool success);
    void onExtractionError(const QString &error);

  private:
    Ui::MainWindow *ui;
    QString backupFilename;
    QString filePassword;
    QString currentExtractDir;
    QString lastExtractionError;
    ExtractionWorker *activeWorker = nullptr;
    void showInGraphicalShell(const QString &pathIn);
};

#endif // MAINWINDOW_H
