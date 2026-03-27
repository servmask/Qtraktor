#ifndef AUTOEXTRACTOR_H
#define AUTOEXTRACTOR_H

#include <QObject>
#include <QStringList>
#include <QLocalServer>
#include <QProgressDialog>
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
    void onWorkerProgress(float percent);
    void onWorkerPhaseChanged(const QString &phase);
    void onWorkerError(const QString &message);
    void onWorkerFinished(bool success);
    void onProgressTimeout();

private:
    void showNotification(const QString &title, const QString &message);
    QString resolveDestDir(const QString &sourceFilePath);

    QStringList m_queue;
    QLocalServer *m_server;
    ExtractionWorker *m_worker;
    QProgressDialog *m_progressDialog;
    QSystemTrayIcon *m_trayIcon;
    QTimer *m_progressTimer;
    QString m_currentFile;
    QString m_currentDestDir;
    QString m_lastError;
    bool m_shuttingDown;
};

#endif // AUTOEXTRACTOR_H
