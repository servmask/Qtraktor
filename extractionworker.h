#ifndef EXTRACTIONWORKER_H
#define EXTRACTIONWORKER_H

#include <QThread>
#include <QAtomicInt>
#include <QString>
#include "cryptoutils.h"

struct CheckResult {
    bool isEncrypted = false;
    bool isValid = false;
    bool isV2 = false;
    CompressionType compressionType = COMPRESSION_NONE;
};

class ExtractionWorker : public QThread
{
    Q_OBJECT

public:
    ExtractionWorker(const QString &filePath, const QString &password,
                     const QString &destDir, QObject *parent = nullptr);

    void abort();
    bool isAborted() const;
    float currentProgress() const;

    static CheckResult checkConfig(const QString &filePath);

signals:
    void progress(float percent);
    void phaseChanged(const QString &phase);
    void extractionError(const QString &message);
    void extractionFinished(bool success);
    void logMessage(const QString &message);

protected:
    void run() override;

private:
    QString m_filePath;
    QString m_password;
    QString m_destDir;
    QAtomicInt m_abort;
    QAtomicInt m_progress; // 0-100, set from worker, polled from main thread
};

#endif // EXTRACTIONWORKER_H
