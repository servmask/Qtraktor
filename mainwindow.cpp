#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QIODevice>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QProcess>
#include "passworddialog.h"
#include "cryptoutils.h"
#include <QFileOpenEvent>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  ui->progressBar->setVisible(false);
  ui->logTextEdit->setVisible(false);

  connect(ui->dropZone, &DropOverlay::fileDropped, this, &MainWindow::openBackupFile);
  connect(ui->dropZone, &DropOverlay::clicked, this, &MainWindow::openBackup);
  ui->clearButton->setVisible(false);
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::openBackup()
{
  backupFilename = QFileDialog::getOpenFileName(
    this,
    tr("Open a backup"),
    "",
    tr("WordPress backup (*.wpress)")
  );

  if (backupFilename.isNull()) {
    return;
  }

  QFileInfo fileInfo(backupFilename);

  if (!fileInfo.isReadable()) {
    QMessageBox::warning(
      this,
      tr("Unable to open file"),
      tr("Unable to open file: %1").arg(backupFilename),
      QMessageBox::StandardButton::Ok
    );
    return;
  }

  openBackupFile(backupFilename);
  filePassword.clear(); // Reset password when opening new file
}

void MainWindow::clearFile()
{
  backupFilename.clear();
  filePassword.clear();
  ui->dropZone->setFileName(QString());
  ui->extractBackupButton->setEnabled(false);
  ui->clearButton->setVisible(false);
  ui->progressBar->setVisible(false);
  ui->logTextEdit->setVisible(false);
}

void MainWindow::extractTo()
{
  QString extractToDir = QFileDialog::getExistingDirectory(
    this,
    tr("Select extract to folder"),
    ""
  );

  if (extractToDir.isNull()) {
    return;
  }

  QFileInfo fileInfo(backupFilename);
  QDir extractTo(extractToDir + "/" + fileInfo.baseName());

  if (!QDir().mkdir(extractTo.path())) {
     QMessageBox::warning(
      this,
      tr("Unable to create directory"),
      tr("Unable to create directory %1. Fix permissions and try again.").arg(extractTo.path()),
      QMessageBox::StandardButton::Ok
    );
    return;
  }


  BackupFile configChecker(backupFilename);
  bool needsPassword = false;
  CompressionType compressionType = COMPRESSION_NONE;

  if (configChecker.open(QIODevice::ReadOnly)) {
    if (configChecker.isValid()) {

      needsPassword = configChecker.isEncryptedFile();
      compressionType = configChecker.getCompressionType();
      configChecker.close();

      if (needsPassword && filePassword.isEmpty()) {

        PasswordDialog dialog(this);
        dialog.setWindowTitle(tr("Password Required"));

        if (dialog.exec() == QDialog::Accepted) {
          filePassword = dialog.getPassword();
        } else {
          extractTo.removeRecursively();
          return;
        }
      }
    } else {
      configChecker.close();
    }
  }

  BackupFile backupFile(backupFilename, filePassword);

  if (!backupFile.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(
      this,
      tr("Unable to open file"),
      tr("Unable to open file %1 for reading. Fix permissions and try again.").arg(backupFilename),
      QMessageBox::StandardButton::Ok
    );
    return;
  }
  backupFile.setConfig(needsPassword, compressionType);

  if (!backupFile.isValid()) {
     QMessageBox::warning(
      this,
      tr("Corrupted backup file"),
      tr("The backup file is corrupted. It is missing the end of the file."),
      QMessageBox::StandardButton::Ok
    );
    backupFile.close();
    return;
  }

  ui->progressBar->setVisible(true);
  ui->logTextEdit->clear();
  ui->logTextEdit->setVisible(false);

  // Disable controls during extraction
  ui->openBackupButton->setEnabled(false);
  ui->extractBackupButton->setEnabled(false);

  QString lastError;
  connect(&backupFile, &BackupFile::progress, this, &MainWindow::extractProgress);
  connect(&backupFile, &BackupFile::error, this, [&lastError](const QString &error) {
    lastError = error;
  });

  connect(&backupFile, &BackupFile::logMessage, this, [this](const QString &msg) {
    ui->logTextEdit->setVisible(true);
    ui->logTextEdit->append(msg);
  });

  bool extractionSuccess = backupFile.extract(extractTo);
  backupFile.close();

  // Re-enable generic controls
  ui->openBackupButton->setEnabled(true);

  if (!extractionSuccess) {
    ui->progressBar->setVisible(false);
    ui->extractBackupButton->setEnabled(true); // Re-enable extract on failure

    filePassword.clear();
    extractTo.removeRecursively();

    QString errorMessage = tr(
      "The backup file extraction failed. The file may be corrupted or the password may be incorrect.");
    if (!lastError.isEmpty()) {
      errorMessage += "\n\n" + lastError;
    }
    QMessageBox::warning(
      this,
      tr("Extraction failed"),
      errorMessage,
      QMessageBox::StandardButton::Ok
    );
  } else {
    ui->progressBar->setVisible(false);
    ui->dropZone->setFileName(tr("Extracted backup in %1").arg(extractTo.path()));
    ui->extractBackupButton->setDisabled(true);
    showInGraphicalShell(extractTo.path());
  }
}

void MainWindow::extractProgress(float percent)
{
  ui->progressBar->setValue(static_cast<int>(percent));
}

// copied form https://github.com/qt-creator/qt-creator/blob/master/src/plugins/coreplugin/fileutils.cpp#L67
void MainWindow::showInGraphicalShell(const QString &pathIn)
{
    const QFileInfo fileInfo(pathIn);

#if defined (Q_OS_WIN)
  QStringList param;
  if (!fileInfo.isDir())
    param += QLatin1String("/select,");
  param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
  QProcess::startDetached("explorer", param);
#endif

#if defined (Q_OS_MAC)
  QStringList scriptArgs;
  scriptArgs << QLatin1String("-e")
      << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
      .arg(fileInfo.canonicalFilePath());
  QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
  scriptArgs.clear();
  scriptArgs << QLatin1String("-e")
      << QLatin1String("tell application \"Finder\" to activate");
  QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
#endif
}

void MainWindow::openBackupFile(const QString &filename)
{
    backupFilename = filename;
    QFileInfo fileInfo(backupFilename);

    if (!fileInfo.isReadable()) {
        QMessageBox::warning(this, tr("Unable to open file"),
                           tr("Unable to open file: %1").arg(backupFilename),
                           QMessageBox::StandardButton::Ok);
        return;
    }

    ui->dropZone->setFileName(fileInfo.fileName());
    ui->extractBackupButton->setEnabled(true);
    ui->clearButton->setVisible(true);
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        openBackupFile(openEvent->file());
        return true;
    }
    return QMainWindow::event(event);
}
