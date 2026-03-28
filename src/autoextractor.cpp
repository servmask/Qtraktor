#include "autoextractor.h"
#include "dockprogress.h"
#include <QApplication>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "passworddialog.h"

AutoExtractor::AutoExtractor(const QStringList &files, QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this)), m_worker(nullptr), m_progressDialog(nullptr),
      m_trayIcon(nullptr), m_pollTimer(new QTimer(this)), m_shuttingDown(false)
{
    m_queue = files;
    m_pollTimer->setInterval(50); // poll 20 times/sec
    connect(m_pollTimer, &QTimer::timeout, this, &AutoExtractor::pollProgress);

    const QString socketName = "com.servmask.Traktor";
    QLocalServer::removeServer(socketName);
    if (m_server->listen(socketName)) {
        connect(m_server, &QLocalServer::newConnection, this, &AutoExtractor::onNewConnection);
    }

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
            if (!m_worker) {
                processQueue();
            }
        }
    }
}

void AutoExtractor::processQueue()
{

    if (m_queue.isEmpty() && !m_worker) {
        m_shuttingDown = true;
        m_server->close();
        if (m_progressDialog)
            m_progressDialog->hide();
        QApplication::quit();
        return;
    }

    if (m_shuttingDown || m_worker || m_queue.isEmpty())
        return;

    m_currentFile = m_queue.takeFirst();
    m_lastError.clear();

    QFileInfo fileInfo(m_currentFile);
    if (!fileInfo.isReadable()) {
        QMessageBox::warning(nullptr, tr("Unable to open file"), tr("Unable to open file: %1").arg(m_currentFile));
        QTimer::singleShot(0, this, &AutoExtractor::processQueue);
        return;
    }

    CheckResult config = ExtractionWorker::checkConfig(m_currentFile);

    if (!config.isValid) {
        QMessageBox::warning(nullptr, tr("Corrupted backup file"),
                             tr("The backup file is corrupted: %1").arg(m_currentFile));
        QTimer::singleShot(0, this, &AutoExtractor::processQueue);
        return;
    }

    m_currentFileEncrypted = config.isEncrypted;

    QString password;
    if (m_currentFileEncrypted) {
        PasswordDialog dialog;
        dialog.setWindowTitle(tr("Password Required - %1").arg(fileInfo.fileName()));
        if (dialog.exec() == QDialog::Accepted) {
            password = dialog.getPassword();
        } else {
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

    startExtraction(password);
}

void AutoExtractor::startExtraction(const QString &password)
{
    if (!m_progressDialog)
        createProgressDialog();

    QFileInfo fileInfo(m_currentFile);
    m_progressLabel->setText(tr("Extracting \"%1\"").arg(fileInfo.fileName()));
    m_progressBar->setValue(0);

    if (!m_queue.isEmpty()) {
        m_progressDialog->setWindowTitle(tr("Traktor (%1 more)").arg(m_queue.size()));
    } else {
        m_progressDialog->setWindowTitle(tr("Traktor"));
    }

    m_progressDialog->show();
    m_progressDialog->raise();

    m_worker = new ExtractionWorker(m_currentFile, password, m_currentDestDir, this);
    connect(m_worker, &ExtractionWorker::extractionError, this, &AutoExtractor::onWorkerError);
    connect(m_worker, &ExtractionWorker::extractionFinished, this, &AutoExtractor::onWorkerFinished);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);

    m_pollTimer->start();
    m_worker->start();
}

void AutoExtractor::createProgressDialog()
{
    m_progressDialog = new QDialog();
    m_progressDialog->setWindowTitle(tr("Traktor"));
    m_progressDialog->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    m_progressDialog->setFixedWidth(380);

    m_progressLabel = new QLabel;
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
#ifdef Q_OS_MAC
    // Qt 5 QProgressBar native rendering is broken on macOS 26+
    m_progressBar->setFixedHeight(8);
    m_progressBar->setStyleSheet("QProgressBar { border: none; border-radius: 4px; background: palette(midlight); }"
                                 "QProgressBar::chunk { border-radius: 4px; background: palette(highlight); }");
#endif

    m_stopButton = new QPushButton(tr("Stop"));
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (m_worker)
            m_worker->abort();
    });

    QHBoxLayout *barRow = new QHBoxLayout;
    barRow->setSpacing(8);
    barRow->addWidget(m_progressBar, 1);
    barRow->addWidget(m_stopButton);

    QVBoxLayout *layout = new QVBoxLayout(m_progressDialog);
    layout->addWidget(m_progressLabel);
    layout->addLayout(barRow);
}

void AutoExtractor::pollProgress()
{
    if (!m_worker)
        return;

    const int pct = static_cast<int>(m_worker->currentProgress());
    if (m_progressBar) {
        m_progressBar->setValue(pct);
    }
    setDockBadge(QString::number(pct) + "%");
}

void AutoExtractor::onWorkerPhaseChanged(const QString &phase)
{
    Q_UNUSED(phase);
}

void AutoExtractor::onWorkerError(const QString &message)
{
    m_lastError = message;
}

void AutoExtractor::onWorkerFinished(bool success)
{

    m_pollTimer->stop();
    m_worker = nullptr;
    clearDockBadge();

    if (m_progressDialog) {
        m_progressDialog->hide();
    }

    if (success) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentDestDir));
    } else {
        // Clean up the failed extraction directory
        QDir(m_currentDestDir).removeRecursively();

        // If encrypted, ask for password again instead of showing error
        if (m_currentFileEncrypted) {
            PasswordDialog dialog;
            dialog.setWindowTitle(tr("Incorrect password - %1").arg(QFileInfo(m_currentFile).fileName()));
            if (dialog.exec() == QDialog::Accepted) {
                // Re-create destination directory and retry
                m_currentDestDir = resolveDestDir(m_currentFile);
                if (!m_currentDestDir.isEmpty()) {
                    m_lastError.clear();
                    startExtraction(dialog.getPassword());
                    return;
                }
            }
            // User cancelled or couldn't create directory, move on
        } else if (!m_lastError.isEmpty()) {
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
    m_currentFileEncrypted = false;

    QTimer::singleShot(0, this, &AutoExtractor::processQueue);
}

void AutoExtractor::showNotification(const QString &title, const QString &message)
{
    Q_UNUSED(title);
    Q_UNUSED(message);
}

QString AutoExtractor::resolveDestDir(const QString &sourceFilePath)
{
    QFileInfo fi(sourceFilePath);
    QString baseDir = fi.absolutePath();

    if (!QFileInfo(baseDir).isWritable()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (baseDir.isEmpty())
            return QString();
    }

    QString destPath = baseDir + "/" + fi.baseName();

    if (QDir(destPath).exists()) {
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
