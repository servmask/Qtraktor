#include "extractionworker.h"
#include "backupfile.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

ExtractionWorker::ExtractionWorker(const QString &filePath, const QString &password,
                                   const QString &destDir, QObject *parent)
    : QThread(parent),
      m_filePath(filePath),
      m_password(password),
      m_destDir(destDir),
      m_abort(0),
      m_progress(0)
{
}

float ExtractionWorker::currentProgress() const
{
    return static_cast<float>(m_progress.loadAcquire());
}

void ExtractionWorker::abort()
{
    m_abort.storeRelease(1);
}

bool ExtractionWorker::isAborted() const
{
    return m_abort.loadAcquire() != 0;
}

CheckResult ExtractionWorker::checkConfig(const QString &filePath)
{
    CheckResult result;

    BackupFile checker(filePath);
    if (!checker.open(QIODevice::ReadOnly))
        return result;

    result.isValid = checker.isValid();
    if (result.isValid) {
        result.isEncrypted = checker.isEncryptedFile();
        result.compressionType = checker.getCompressionType();
        result.isV2 = checker.isV2Format();
    }

    checker.close();
    return result;
}

void ExtractionWorker::run()
{
    // Open and validate on the worker thread
    BackupFile backupFile(m_filePath, m_password);
    backupFile.setAbortFlag(&m_abort);

    if (!backupFile.open(QIODevice::ReadOnly)) {
        emit extractionError(tr("Unable to open file %1 for reading.").arg(m_filePath));
        emit extractionFinished(false);
        return;
    }

    backupFile.ensureConfigLoaded();
    backupFile.setConfig(backupFile.isEncryptedFile(), backupFile.getCompressionType());

    if (!backupFile.isValid()) {
        emit extractionError(tr("The backup file is corrupted (missing end-of-file marker)."));
        backupFile.close();
        emit extractionFinished(false);
        return;
    }

    // CRC verification for v2 archives
    if (backupFile.isV2Format()) {
        emit phaseChanged(tr("Verifying %1...").arg(QFileInfo(m_filePath).fileName()));

        if (!backupFile.verifyArchiveCrc()) {
            // verifyArchiveCrc emits its own error
            backupFile.close();
            emit extractionFinished(false);
            return;
        }
    }

    emit phaseChanged(tr("Extracting %1...").arg(QFileInfo(m_filePath).fileName()));

    // Store progress atomically for main-thread polling (cross-thread signals unreliable with QThread)
    connect(&backupFile, &BackupFile::progress, this, [this](float p) {
        m_progress.storeRelease(static_cast<int>(p));
    }, Qt::DirectConnection);
    connect(&backupFile, &BackupFile::error, this, [this](const QString &msg) {
        emit extractionError(msg);
    }, Qt::DirectConnection);
    connect(&backupFile, &BackupFile::logMessage, this, [this](const QString &msg) {
        emit logMessage(msg);
    }, Qt::DirectConnection);

    QDir destDir(m_destDir);
    bool ok = backupFile.extract(destDir);
    backupFile.close();

    emit extractionFinished(ok);
}
