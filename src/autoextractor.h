#ifndef AUTOEXTRACTOR_H
#define AUTOEXTRACTOR_H

#include <QObject>
#include <QStringList>
#include <QDialog>
#include <QLabel>
#include <QLocalServer>
#include <QProgressBar>
#include <QPushButton>
#include <QSystemTrayIcon>
#include "extractionworker.h"

class AutoExtractor : public QObject
{
    Q_OBJECT

public:
    explicit AutoExtractor(const QStringList &files, QObject *parent = nullptr);
    ~AutoExtractor();

    void enqueueFile(const QString &filePath);

private slots:
    void processQueue();
    void onNewConnection();
    void pollProgress();
    void onWorkerPhaseChanged(const QString &phase);
    void onWorkerError(const QString &message);
    void onWorkerFinished(bool success);

private:
    void showNotification(const QString &title, const QString &message);
    QString resolveDestDir(const QString &sourceFilePath);

    QStringList m_queue;
    QLocalServer *m_server;
    void createProgressDialog();

    ExtractionWorker *m_worker;
    QDialog *m_progressDialog;
    QLabel *m_progressLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_stopButton;
    QSystemTrayIcon *m_trayIcon;
    QTimer *m_pollTimer;
    void startExtraction(const QString &password);

    QString m_currentFile;
    QString m_currentDestDir;
    QString m_lastError;
    bool m_shuttingDown;
    bool m_currentFileEncrypted;
};

#endif // AUTOEXTRACTOR_H
