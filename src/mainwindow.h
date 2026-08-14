#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUrl>
#include "backupfile.h"
#include "extractionworker.h"
#include "installcli.h"
#include "passworddialog.h"

namespace Ui
{
class MainWindow;
}

class CloudDownloader;
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
    void openBackupFromUrl(const QUrl &url);
    void setPassword(const QString &password);
    void clearFile();
    void extractTo();
    void extractToPath(const QString &destDir);
    void extractProgress(float percent);

private slots:
    void onExtractionFinished(bool success);
    void onExtractionError(const QString &error);
    void onDownloadProgress(int percent);
    void onDownloadFinished(const QString &tempFilePath, const QString &suggestedName);
    void onDownloadFailed(const QString &errorMessage);
    void cancelDownload();
    void openFromUrl();
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
    CloudDownloader *m_downloader = nullptr;
    bool m_downloading = false;
    QString m_pendingTempFile;                // temp file to remove on next open/clear
    QString m_loadedDisplayName;              // friendly label of the loaded backup (temp paths are ugly)
    UpdateManager *m_updateManager = nullptr; // non-null on macOS and Windows; nullptr on Linux
    void showInGraphicalShell(const QString &pathIn);
    void setDownloadingState(bool downloading);
    bool isBusy() const; // true while a download OR an extraction is running
};

#endif // MAINWINDOW_H
