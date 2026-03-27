#include "autoextractor.h"
#include "dockprogress.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "passworddialog.h"

AutoExtractor::AutoExtractor(const QStringList &files, QObject *parent)
    : QObject(parent),
      m_server(new QLocalServer(this)),
      m_worker(nullptr),
      m_progressDialog(nullptr),
      m_trayIcon(nullptr),
      m_progressTimer(new QTimer(this)),
      m_shuttingDown(false)
{
    m_queue = files;
    m_progressTimer->setSingleShot(true);
    connect(m_progressTimer, &QTimer::timeout, this, &AutoExtractor::onProgressTimeout);

    // Set up single-instance IPC server
    const QString socketName = "com.servmask.Traktor";
    QLocalServer::removeServer(socketName);
    if (m_server->listen(socketName)) {
        connect(m_server, &QLocalServer::newConnection, this, &AutoExtractor::onNewConnection);
    }

    // Start processing after event loop begins
    QTimer::singleShot(0, this, &AutoExtractor::processQueue);
}

AutoExtractor::~AutoExtractor()
{
    delete m_progressDialog;
    delete m_trayIcon;
}

void AutoExtractor::enqueueFile(const QString &filePath)
{
    if (m_shuttingDown)
        return;
    m_queue.append(filePath);
}

void AutoExtractor::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        socket->waitForReadyRead(1000);
        const QByteArray data = socket->readAll();
        socket->close();
        socket->deleteLater();

        const QString filePath = QString::fromUtf8(data).trimmed();
        if (!filePath.isEmpty() && filePath.endsWith(".wpress", Qt::CaseInsensitive)) {
            enqueueFile(filePath);
            // If no worker is active, start processing
            if (!m_worker) {
                processQueue();
            }
        }
    }
}

void AutoExtractor::processQueue()
{
    if (m_queue.isEmpty() && !m_worker) {
        // All done, quit
        m_shuttingDown = true;
        m_server->close();
        QApplication::quit();
        return;
    }

    if (m_shuttingDown || m_worker || m_queue.isEmpty())
        return;

    m_currentFile = m_queue.takeFirst();
    m_lastError.clear();

    QFileInfo fileInfo(m_currentFile);
    if (!fileInfo.isReadable()) {
        QMessageBox::warning(nullptr, tr("Unable to open file"),
                           tr("Unable to open file: %1").arg(m_currentFile));
        QTimer::singleShot(0, this, &AutoExtractor::processQueue);
        return;
    }

    // Quick config check to determine if password is needed
    CheckResult config = ExtractionWorker::checkConfig(m_currentFile);

    if (!config.isValid) {
        QMessageBox::warning(nullptr, tr("Corrupted backup file"),
                           tr("The backup file is corrupted: %1").arg(m_currentFile));
        QTimer::singleShot(0, this, &AutoExtractor::processQueue);
        return;
    }

    QString password;
    if (config.isEncrypted) {
        PasswordDialog dialog;
        dialog.setWindowTitle(tr("Password Required - %1").arg(fileInfo.fileName()));
        if (dialog.exec() == QDialog::Accepted) {
            password = dialog.getPassword();
        } else {
            // User cancelled, skip this file
            QTimer::singleShot(0, this, &AutoExtractor::processQueue);
            return;
        }
    }

    m_currentDestDir = resolveDestDir(m_currentFile);
    if (m_currentDestDir.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Unable to create directory"),
                           tr("Unable to create extraction directory for %1").arg(m_currentFile));
        QTimer::singleShot(0, this, &AutoExtractor::processQueue);
        return;
    }

    // Create and start the worker
    m_worker = new ExtractionWorker(m_currentFile, password, m_currentDestDir, this);
    connect(m_worker, &ExtractionWorker::progress, this, &AutoExtractor::onWorkerProgress);
    connect(m_worker, &ExtractionWorker::phaseChanged, this, &AutoExtractor::onWorkerPhaseChanged);
    connect(m_worker, &ExtractionWorker::extractionError, this, &AutoExtractor::onWorkerError);
    connect(m_worker, &ExtractionWorker::extractionFinished, this, &AutoExtractor::onWorkerFinished);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);

    // Start 2-second timer for delayed progress window
    m_progressTimer->start(2000);

    m_worker->start();
}

void AutoExtractor::onProgressTimeout()
{
    // Only show if worker is still running
    if (!m_worker)
        return;

    if (!m_progressDialog) {
        m_progressDialog = new QProgressDialog();
        m_progressDialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
        m_progressDialog->setMinimumDuration(0);
        m_progressDialog->setAutoClose(false);
        m_progressDialog->setAutoReset(false);
        m_progressDialog->setCancelButtonText(tr("Cancel"));
        m_progressDialog->setMinimum(0);
        m_progressDialog->setMaximum(100);

        connect(m_progressDialog, &QProgressDialog::canceled, this, [this]() {
            if (m_worker) {
                m_worker->abort();
            }
        });
    }

    QFileInfo fi(m_currentFile);
    m_progressDialog->setLabelText(tr("Extracting %1...").arg(fi.fileName()));

    QString remaining;
    if (!m_queue.isEmpty()) {
        remaining = tr(" (%1 remaining)").arg(m_queue.size());
    }
    m_progressDialog->setWindowTitle(tr("Traktor%1").arg(remaining));
    m_progressDialog->show();
}

void AutoExtractor::onWorkerProgress(float percent)
{
    const int pct = static_cast<int>(percent);
    if (m_progressDialog && m_progressDialog->isVisible()) {
        m_progressDialog->setValue(pct);
    }
    setDockBadge(QString::number(pct) + "%");
}

void AutoExtractor::onWorkerPhaseChanged(const QString &phase)
{
    if (m_progressDialog && m_progressDialog->isVisible()) {
        m_progressDialog->setLabelText(phase);
    }
}

void AutoExtractor::onWorkerError(const QString &message)
{
    m_lastError = message;
}

void AutoExtractor::onWorkerFinished(bool success)
{
    m_progressTimer->stop();
    m_worker = nullptr;
    clearDockBadge();

    if (m_progressDialog) {
        m_progressDialog->hide();
    }

    if (success) {
        // Open the extracted folder
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentDestDir));

        // Show notification
        showNotification(tr("Extraction Complete"),
                        tr("%1 extracted successfully.").arg(QFileInfo(m_currentFile).fileName()));
    } else {
        // Clean up partial directory
        QDir(m_currentDestDir).removeRecursively();

        if (!m_lastError.isEmpty()) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Extraction Failed"));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setTextFormat(Qt::RichText);
            msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
            msgBox.setText(m_lastError);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
        }
    }

    m_lastError.clear();
    m_currentFile.clear();
    m_currentDestDir.clear();

    // Process next in queue
    QTimer::singleShot(0, this, &AutoExtractor::processQueue);
}

void AutoExtractor::showNotification(const QString &title, const QString &message)
{
    if (!m_trayIcon) {
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setIcon(QApplication::windowIcon());
    }

    if (QSystemTrayIcon::supportsMessages()) {
        m_trayIcon->show();
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 5000);
        // Hide tray icon after notification is shown to keep app invisible
        QTimer::singleShot(6000, m_trayIcon, &QSystemTrayIcon::hide);
    }
}

QString AutoExtractor::resolveDestDir(const QString &sourceFilePath)
{
    QFileInfo fi(sourceFilePath);
    QString baseDir = fi.absolutePath();

    // Check if source directory is writable
    if (!QFileInfo(baseDir).isWritable()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (baseDir.isEmpty())
            return QString();
    }

    QString destPath = baseDir + "/" + fi.baseName();

    if (QDir(destPath).exists()) {
        // Silently create numbered variant
        int suffix = 1;
        while (QDir(destPath + " (" + QString::number(suffix) + ")").exists() && suffix <= 100) {
            suffix++;
        }
        if (suffix > 100)
            return QString();
        destPath = destPath + " (" + QString::number(suffix) + ")";
    }

    if (!QDir().mkdir(destPath))
        return QString();

    return destPath;
}
