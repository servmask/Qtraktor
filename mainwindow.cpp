#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QIODevice>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QProcess>
#include <QMimeData>
#include <QDropEvent>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  ui->progressBar->setVisible(false);
  setAcceptDrops(true);
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

  ui->backupNameLabel->setText(fileInfo.fileName());
  ui->extractBackupButton->setEnabled(true);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event)
{
  QList<QUrl> urls = event->mimeData()->urls();
  if (urls.isEmpty()) {
    return;
  }

  backupFilename = urls.first().toLocalFile();
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

  ui->backupNameLabel->setText(fileInfo.fileName());
  ui->extractBackupButton->setEnabled(true);
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

  BackupFile backupFile(backupFilename);
  if (!backupFile.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(
      this,
      tr("Unable to open file"),
      tr("Unable to open file %1 for reading. Fix permissions and try again.").arg(backupFilename),
      QMessageBox::StandardButton::Ok
    );
    return;
  }

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

  ui->backupNameLabel->setVisible(false);
  ui->progressBar->setVisible(true);

  connect(&backupFile, &BackupFile::progress, this, &MainWindow::extractProgress);

  if (!backupFile.extract(extractTo)) {
      QMessageBox::warning(
      this,
      tr("Corrupted backup file"),
      tr("The backup file is corrupted."),
      QMessageBox::StandardButton::Ok
    );
  }

  ui->progressBar->setVisible(false);
  ui->backupNameLabel->setText(tr("Extracted backup in %1").arg(extractTo.path()));
  ui->backupNameLabel->setVisible(true);
  ui->extractBackupButton->setDisabled(true);
  showInGraphicalShell(extractTo.path());
  backupFile.close();
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
