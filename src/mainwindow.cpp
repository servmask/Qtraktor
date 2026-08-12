#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QIcon>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include "agentconfig.h"
#include "clouddownloader.h"
#include "installcli.h"
#include "passworddialog.h"
#include "setupdialog.h"
#include "aboutdialog.h"
#include "cryptoutils.h"
#include "urlopendialog.h"

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
#include "updatemanager.h"
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    // Set the app icon globally so QApplication::windowIcon() returns it
    // for AboutDialog and any other window that asks. Bundled via the
    // resources.qrc Qt resource so it works the same on all platforms.
    qApp->setWindowIcon(QIcon(QStringLiteral(":/icons/traktor.png")));

    ui->setupUi(this);
    ui->progressBar->setVisible(false);
    ui->logTextEdit->setVisible(false);
    connect(ui->dropZone, &DropOverlay::fileDropped, this, &MainWindow::openBackupFile);
    connect(ui->dropZone, &DropOverlay::urlDropped, this, &MainWindow::openBackupFromUrl);
    connect(ui->dropZone, &DropOverlay::clicked, this, &MainWindow::openBackup);
    ui->clearButton->setVisible(false);

    m_downloader = new CloudDownloader(this);
    connect(m_downloader, &CloudDownloader::progress, this, &MainWindow::onDownloadProgress);
    connect(m_downloader, &CloudDownloader::finished, this, &MainWindow::onDownloadFinished);
    connect(m_downloader, &CloudDownloader::failed, this, &MainWindow::onDownloadFailed);

    ui->cancelButton->setVisible(false);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::cancelDownload);

    // Insert "Open from URL…" into the existing File menu (defined in mainwindow.ui),
    // right after the "Open backup" action so it appears as the second item.
    auto *openFromUrlAction = new QAction(tr("Open from &URL..."), this);
    openFromUrlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    ui->menu_File->insertAction(ui->actionClearFile, openFromUrlAction);
    connect(openFromUrlAction, &QAction::triggered, this, &MainWindow::openFromUrl);

    // Add Tools menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *installCliAction = toolsMenu->addAction(tr("Install Command Line Tool..."));
    connect(installCliAction, &QAction::triggered, this, &MainWindow::installCliTool);
    QAction *manageAgentsAction = toolsMenu->addAction(tr("Manage AI Agent Integrations..."));
    connect(manageAgentsAction, &QAction::triggered, this, &MainWindow::manageAgentIntegrations);
    toolsMenu->addSeparator();
    QAction *uninstallAction = toolsMenu->addAction(tr("Uninstall Traktor..."));
    connect(uninstallAction, &QAction::triggered, this, &MainWindow::uninstallTraktor);

    // "About Traktor" - universal across platforms.
    // setMenuRole(AboutRole) routes this into the macOS app menu's About
    // slot; on Windows/Linux it stays in the Tools menu where added.
    QAction *aboutAction = new QAction(tr("About Traktor"), this);
    aboutAction->setMenuRole(QAction::AboutRole);
    toolsMenu->addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutDialog dlg(this);
        dlg.exec();
    });

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    // Auto-update bridge: Sparkle on macOS, WinSparkle on Windows. Both
    // backends start their scheduled check loop on construction and read
    // their config from a platform-native source (Info.plist on macOS,
    // hardcoded API calls in updatemanager_win.cpp on Windows).
    m_updateManager = new UpdateManager(this);

    // "Check for Updates..." - on macOS, setMenuRole(ApplicationSpecificRole)
    // tells Qt to relocate this into the app menu (Traktor → ...) right
    // below the About item. On Windows the role is a no-op so the action
    // stays in the Tools menu where added.
    //
    // The check call is idempotent on both platforms - if a check is
    // already in flight, the framework re-fronts its existing progress
    // window - so we don't need to disable the action while busy.
    QAction *checkForUpdatesAction = new QAction(tr("Check for Updates..."), this);
#ifdef Q_OS_MAC
    checkForUpdatesAction->setMenuRole(QAction::ApplicationSpecificRole);
#endif
    toolsMenu->addAction(checkForUpdatesAction);
    connect(checkForUpdatesAction, &QAction::triggered, m_updateManager, &UpdateManager::checkForUpdates);
#endif

    // First-run setup dialog
    QSettings firstRunSettings("com.servmask", "Traktor");
    if (!firstRunSettings.value("setupComplete", false).toBool()) {
        QTimer::singleShot(500, this, [this]() {
            SetupDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                QSettings s("com.servmask", "Traktor");
                s.setValue("setupComplete", true);
            }
        });
    }

    QSettings settings("com.servmask", "Traktor");
    restoreGeometry(settings.value("windowGeometry").toByteArray());
}

MainWindow::~MainWindow()
{
    QSettings settings("com.servmask", "Traktor");
    settings.setValue("windowGeometry", saveGeometry());
    delete ui;
}

void MainWindow::openBackup()
{
    if (isBusy())
        return;

    QSettings settings("com.servmask", "Traktor");
    QString lastDir = settings.value("lastOpenPath").toString();

    QString selectedFile =
        QFileDialog::getOpenFileName(this, tr("Open a backup"), lastDir, tr("WordPress backup (*.wpress)"));

    if (selectedFile.isNull()) {
        return;
    }

    settings.setValue("lastOpenPath", QFileInfo(selectedFile).absolutePath());
    backupFilename = selectedFile;

    QFileInfo fileInfo(backupFilename);

    if (!fileInfo.isReadable()) {
        QMessageBox::warning(this, tr("Unable to open file"), tr("Unable to open file: %1").arg(backupFilename),
                             QMessageBox::StandardButton::Ok);
        return;
    }

    openBackupFile(backupFilename);
    filePassword.clear(); // Reset password when opening new file
}

void MainWindow::clearFile()
{
    if (m_downloading && m_downloader)
        m_downloader->abort(); // stop any in-flight download; leaves no partial temp file behind

    backupFilename.clear();
    filePassword.clear();
    setDownloadingState(false); // re-enable Open / Open-from-URL and reset busy state
    ui->dropZone->setFileName(QString());
    ui->extractBackupButton->setEnabled(false);
    ui->clearButton->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->logTextEdit->setVisible(false);

    // Remove temp file from a previous cloud download
    if (!m_pendingTempFile.isEmpty()) {
        QFile::remove(m_pendingTempFile);
        m_pendingTempFile.clear();
    }
}

void MainWindow::extractTo()
{
    QSettings settings("com.servmask", "Traktor");
    QString lastDir = settings.value("lastExtractPath").toString();

    QString extractToDir = QFileDialog::getExistingDirectory(this, tr("Select extract to folder"), lastDir);

    if (extractToDir.isNull()) {
        return;
    }

    settings.setValue("lastExtractPath", extractToDir);
    extractToPath(extractToDir);
}

void MainWindow::setPassword(const QString &password)
{
    filePassword = password;
}

void MainWindow::extractToPath(const QString &destDir)
{
    if (isBusy()) // never run two flows at once (download in progress / already extracting)
        return;

    QFileInfo fileInfo(backupFilename);
    QDir extractTo(destDir + "/" + fileInfo.baseName());

    if (extractTo.exists()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Directory already exists"));
        msgBox.setText(tr("The directory %1 already exists.").arg(extractTo.path()));
        QPushButton *wipeBtn = msgBox.addButton(tr("Wipe && Extract"), QMessageBox::DestructiveRole);
        QPushButton *newBtn = msgBox.addButton(tr("Create New Folder"), QMessageBox::ActionRole);
        msgBox.addButton(QMessageBox::Cancel);
        msgBox.exec();

        if (msgBox.clickedButton() == wipeBtn) {
            if (!extractTo.removeRecursively()) {
                QMessageBox::warning(
                    this, tr("Unable to remove directory"),
                    tr("Unable to remove directory %1. Fix permissions and try again.").arg(extractTo.path()),
                    QMessageBox::StandardButton::Ok);
                return;
            }
        } else if (msgBox.clickedButton() == newBtn) {
            QString basePath = destDir + "/" + fileInfo.baseName();
            int suffix = 1;
            while (QDir(basePath + " (" + QString::number(suffix) + ")").exists() && suffix <= 100) {
                suffix++;
            }
            if (suffix > 100) {
                QMessageBox::warning(
                    this, tr("Too many directories"),
                    tr("Too many directories with the name %1. Remove some and try again.").arg(fileInfo.baseName()),
                    QMessageBox::StandardButton::Ok);
                return;
            }
            extractTo = QDir(basePath + " (" + QString::number(suffix) + ")");
        } else {
            return;
        }
    }

    if (!QDir().mkdir(extractTo.path())) {
        QMessageBox::warning(this, tr("Unable to create directory"),
                             tr("Unable to create directory %1. Fix permissions and try again.").arg(extractTo.path()),
                             QMessageBox::StandardButton::Ok);
        return;
    }

    // Quick config check on main thread to determine if password is needed
    CheckResult config = ExtractionWorker::checkConfig(backupFilename);

    if (!config.isValid) {
        QMessageBox::warning(this, tr("Corrupted backup file"),
                             tr("The backup file is corrupted. It is missing the end of the file."),
                             QMessageBox::StandardButton::Ok);
        extractTo.removeRecursively();
        return;
    }

    if (config.isEncrypted && filePassword.isEmpty()) {
        PasswordDialog dialog(this);
        dialog.setWindowTitle(tr("Password Required"));

        if (dialog.exec() == QDialog::Accepted) {
            filePassword = dialog.getPassword();
        } else {
            extractTo.removeRecursively();
            return;
        }
    }

    currentExtractDir = extractTo.path();

    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);
    ui->logTextEdit->clear();
    ui->logTextEdit->setVisible(false);

    // Disable controls during extraction (openUrlButton too, so a download can't
    // start mid-extraction and fight this flow over the shared progress bar).
    ui->openBackupButton->setEnabled(false);
    ui->openUrlButton->setEnabled(false);
    ui->extractBackupButton->setEnabled(false);

    activeWorker = new ExtractionWorker(backupFilename, filePassword, currentExtractDir, this);

    connect(activeWorker, &ExtractionWorker::progress, this, &MainWindow::extractProgress);
    connect(activeWorker, &ExtractionWorker::extractionError, this, &MainWindow::onExtractionError);
    connect(activeWorker, &ExtractionWorker::extractionFinished, this, &MainWindow::onExtractionFinished);
    connect(activeWorker, &ExtractionWorker::logMessage, this, [this](const QString &msg) {
        ui->logTextEdit->setVisible(true);
        ui->logTextEdit->append(msg);
    });
    connect(activeWorker, &QThread::finished, activeWorker, &QObject::deleteLater);

    activeWorker->start();
}

void MainWindow::onExtractionError(const QString &error)
{
    lastExtractionError = error;
}

void MainWindow::onExtractionFinished(bool success)
{
    activeWorker = nullptr;
    ui->openBackupButton->setEnabled(true);
    ui->openUrlButton->setEnabled(true);
    ui->progressBar->setVisible(false);

    if (!success) {
        ui->extractBackupButton->setEnabled(true);
        filePassword.clear();
        QDir(currentExtractDir).removeRecursively();

        QString errorMessage;
        if (!lastExtractionError.isEmpty()) {
            errorMessage = lastExtractionError;
        } else {
            errorMessage =
                tr("The backup file extraction failed. The file may be corrupted or the password may be incorrect.");
        }
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Extraction failed"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
        msgBox.setText(errorMessage);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    } else {
        ui->dropZone->setFileName(tr("Extracted backup in %1").arg(currentExtractDir));
        ui->extractBackupButton->setDisabled(true);
        showInGraphicalShell(currentExtractDir);
    }

    lastExtractionError.clear();
}

void MainWindow::extractProgress(float percent)
{
    ui->progressBar->setValue(static_cast<int>(percent));
}

// copied form https://github.com/qt-creator/qt-creator/blob/master/src/plugins/coreplugin/fileutils.cpp#L67
void MainWindow::showInGraphicalShell(const QString &pathIn)
{
    const QFileInfo fileInfo(pathIn);

#if defined(Q_OS_WIN)
    QStringList param;
    if (!fileInfo.isDir())
        param += QLatin1String("/select,");
    param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
    QProcess::startDetached("explorer", param);
#endif

#if defined(Q_OS_MAC)
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
               << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                      .arg(fileInfo.canonicalFilePath());
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e") << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
#endif
}

void MainWindow::openBackupFile(const QString &filename)
{
    if (isBusy()) // e.g. a local file dropped mid-extraction; ignore until idle
        return;

    backupFilename = filename;
    QFileInfo fileInfo(backupFilename);

    if (!fileInfo.isReadable()) {
        QMessageBox::warning(this, tr("Unable to open file"), tr("Unable to open file: %1").arg(backupFilename),
                             QMessageBox::StandardButton::Ok);
        return;
    }

    ui->dropZone->setFileName(fileInfo.fileName());
    ui->extractBackupButton->setEnabled(true);
    ui->clearButton->setVisible(true);
}

void MainWindow::openBackupFromUrl(const QUrl &url)
{
    if (!m_downloader || isBusy())
        return;

    if (!m_pendingTempFile.isEmpty()) {
        QFile::remove(m_pendingTempFile);
        m_pendingTempFile.clear();
    }

    setDownloadingState(true);
    ui->dropZone->setFileName(tr("Downloading from %1\xe2\x80\xa6").arg(url.host()));
    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    m_downloader->download(url);
}

void MainWindow::onDownloadProgress(int percent)
{
    if (percent >= 0) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(percent);
    } else {
        // Content-Length unknown: show a busy/indeterminate bar
        ui->progressBar->setRange(0, 0);
    }
}

void MainWindow::onDownloadFinished(const QString &tempFilePath, const QString &suggestedName)
{
    setDownloadingState(false);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setVisible(false);
    m_pendingTempFile = tempFilePath;
    openBackupFile(tempFilePath);
    ui->dropZone->setFileName(suggestedName);
}

void MainWindow::onDownloadFailed(const QString &errorMessage)
{
    setDownloadingState(false);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setVisible(false);
    // A failed download must not disturb a previously-opened backup: restore its
    // label so Extract stays consistent, instead of blanking the zone while
    // backupFilename still points at the old file.
    if (!backupFilename.isEmpty())
        ui->dropZone->setFileName(QFileInfo(backupFilename).fileName());
    else
        ui->dropZone->setFileName(QString());
    QMessageBox::warning(this, tr("Download Failed"), errorMessage);
}

void MainWindow::cancelDownload()
{
    if (!m_downloading || !m_downloader)
        return;

    m_downloader->abort(); // failed() is NOT emitted after abort(), so restore the UI here
    setDownloadingState(false);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setVisible(false);
    // Restore the previously-open file's label (if any); a cancelled download
    // must not disturb a backup that was already loaded.
    if (!backupFilename.isEmpty())
        ui->dropZone->setFileName(QFileInfo(backupFilename).fileName());
    else
        ui->dropZone->setFileName(QString());
}

void MainWindow::openFromUrl()
{
    if (isBusy())
        return;
    UrlOpenDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    openBackupFromUrl(dlg.url());
}

void MainWindow::setDownloadingState(bool downloading)
{
    m_downloading = downloading;
    ui->openBackupButton->setEnabled(!downloading);
    ui->openUrlButton->setEnabled(!downloading);
    ui->extractBackupButton->setEnabled(!downloading && !backupFilename.isEmpty());
    ui->clearButton->setVisible(!downloading && !backupFilename.isEmpty());
    ui->cancelButton->setVisible(downloading);     // swap Clear ↔ Cancel while a download runs
    ui->actionClearFile->setEnabled(!downloading); // File → Clear File must not fire mid-download
}

bool MainWindow::isBusy() const
{
    return m_downloading || activeWorker != nullptr;
}

void MainWindow::installCliTool()
{
    InstallResult result = installCli();
    if (result.success) {
        QMessageBox::information(this, tr("Install CLI"), result.message);
    } else {
        QMessageBox::warning(this, tr("Install CLI"), result.message);
    }
}

void MainWindow::manageAgentIntegrations()
{
    SetupDialog dialog(this);
    dialog.exec();
}

void MainWindow::uninstallTraktor()
{
    QMessageBox::StandardButton reply =
        QMessageBox::question(this, tr("Uninstall Traktor"),
                              tr("This will remove all AI agent integrations and the command-line tool.\n\n"
                                 "Continue?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Unregister MCP from all agents
    AgentConfigManager mgr;
    QStringList messages;
    mgr.unregisterAll(&messages);

#ifdef Q_OS_MAC
    // Remove CLI symlink
    if (QFile::exists("/usr/local/bin/traktor")) {
        QStringList osascriptArgs;
        osascriptArgs << "-e";
        osascriptArgs << "do shell script \"rm -f /usr/local/bin/traktor\" with administrator privileges";
        QProcess::execute("/usr/bin/osascript", osascriptArgs);
    }
#endif

    // Clear settings
    QSettings("com.servmask", "Traktor").clear();

    QMessageBox::information(this, tr("Uninstall Complete"),
                             tr("Traktor CLI and AI agent integrations have been removed.\n\n"
                                "Drag Traktor.app to Trash to complete uninstall."));
}
