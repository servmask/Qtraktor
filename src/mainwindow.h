#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "backupfile.h"
#include "extractionworker.h"
#include "installcli.h"
#include "passworddialog.h"

namespace Ui
{
class MainWindow;
}

class UpdateManager; // declared in src/updatemanager.h; instantiated on macOS and Windows

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
    void installCliTool();
    void manageAgentIntegrations();
    void uninstallTraktor();

private:
    Ui::MainWindow *ui;
    QString backupFilename;
    QString filePassword;
    QString currentExtractDir;
    QString lastExtractionError;
    ExtractionWorker *activeWorker = nullptr;
    UpdateManager *m_updateManager = nullptr; // non-null on macOS and Windows; nullptr on Linux
    void showInGraphicalShell(const QString &pathIn);
};

#endif // MAINWINDOW_H
