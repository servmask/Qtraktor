#ifndef BACKUPFILE_H
#define BACKUPFILE_H

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QString>
#include <QDateTime>
#include <QRegularExpression>
#include <zlib.h>
#include "cryptoutils.h"

class BackupFile : public QFile
{
  Q_OBJECT

public:
  explicit BackupFile(const QString& filename, const QString& password = QString())
    : QFile(filename),
      bytesRead(0),
      filePassword(password),
      isEncrypted(false),
      isV2(false),
      compressionType(COMPRESSION_NONE)
  {}

  bool isEncryptedFile() const { return isEncrypted; }
  CompressionType getCompressionType() const { return compressionType; }

  void ensureConfigLoaded()
  {
    if (compressionType == COMPRESSION_NONE && !isEncrypted) {
      loadConfig();
    }
  }

  void setConfig(bool encrypted, CompressionType compType)
  {
    isEncrypted = encrypted;
    compressionType = compType;
  }

  bool isValid()
  {
    if (size() == 0) {
      return true;
    }

    loadConfig();

    if (!seek(size() - kHeaderSize)) {
      return false;
    }

    const QByteArray eofBlock = read(kHeaderSize);
    if (!isEofBlock(eofBlock)) {
      return false;
    }

    isV2 = isV2EofBlock(eofBlock);

    if (!seek(0)) {
      return false;
    }

    return true;
  }

  bool extract(QDir extractTo)
  {
    if (size() == 0) {
      return true;
    }

    if (compressionType == COMPRESSION_NONE && !isEncrypted) {
      loadConfig();
    }

    if (isV2 && !verifyArchiveCrc()) {
      return false;
    }

    auto log = [&](const QString& msg, bool isError = false) {
       if (!isError) {
         return;
       }
       QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
       QString formattedMsg = QString("[%1] %2").arg(timestamp, msg);
       emit logMessage(formattedMsg);
       emit error(msg);
    };

    while (!atEnd()) {
      HeaderInfo info;
      if (!readHeader(info)) {
        log("Failed to read file header. The backup file might be corrupted or truncated.", true);
        return false;
      }

      if (info.isEof) {
        return true;
      }

      const QString fullPath = buildOutputPath(extractTo, info.fileName, info.filePath);

      QByteArray fileContent;
      if (!readExact(info.fileSize, fileContent)) {
        log(QString("Fatal: Failed to read content data for file: %1. Archive might be truncated.").arg(info.fileName), true);
        return false;
      }

      QFile out(fullPath);
      if (!out.open(QIODevice::WriteOnly)) {
        log(QString("Error: Failed to create output file: %1 (Permissions?). Skipping.").arg(fullPath), true);
        continue;
      }

      const bool isCompressed = !CryptoUtils::isConfigFile(info.fileName) && compressionType != COMPRESSION_NONE;

      QString processError;
      QByteArray processedContent;

      if (isEncrypted && !filePassword.isEmpty()) {
        processedContent = CryptoUtils::processFileContentWithPassword(
          fileContent, isCompressed, info.fileName, filePassword, compressionType, &processError
        );
      } else {
        processedContent = CryptoUtils::processFileContent(
          fileContent, isCompressed, info.fileName, compressionType, &processError
        );
      }

      if (!processError.isEmpty()) {
        log(QString("Error processing file '%1': %2. Skipping.").arg(info.fileName, processError), true);
        out.close();
        continue;
      }

      if (out.write(processedContent) != processedContent.size()) {
        log(QString("Error: Failed to write to destination file: %1 (Disk full?). Skipping.").arg(fullPath), true);
        out.close();
        continue;
      }

      out.close();

      if (isV2 && !info.crc32.isEmpty()) {
        const QString actualCrc = computeFileCrc32(fullPath);
        if (!actualCrc.isEmpty() && actualCrc != info.crc32) {
          log(QString("CRC mismatch for file '%1': expected %2, got %3").arg(info.fileName, info.crc32, actualCrc), true);
        }
      }
    }

    log("Unexpected end of backup file archive.", true);
    return false;
  }

signals:
  void progress(float percent);
  void error(const QString& errorMessage);
  void logMessage(const QString& msg);

protected:
  qint64 readData(char* data, qint64 maxlen) override
  {
    const qint64 n = QFile::readData(data, maxlen);
    bytesRead += n;

    if (size() > 0) {
      emit progress((static_cast<float>(bytesRead) / static_cast<float>(size())) * 100.0f);
    } else {
      emit progress(0.0f);
    }

    QApplication::processEvents();
    return n;
  }

private:
  static constexpr qint64 kHeaderSize = 4377;
  static constexpr qint64 kCrcChunkSize = 524288;

  struct HeaderInfo
  {
    QString fileName;
    QString filePath;
    QString crc32;
    qint64 fileSize = 0;
    bool isEof = false;
  };

  void loadConfig();

  bool readHeader(HeaderInfo& outInfo)
  {
    const QByteArray header = read(kHeaderSize);
    if (header.size() != kHeaderSize) {
      return false;
    }

    if (isEofBlock(header)) {
      outInfo.isEof = true;
      return true;
    }

    outInfo.isEof = false;
    outInfo.fileName = parseNullTerminatedString(header, 0, 255);

    bool ok = false;
    const QString sizeStr = parseNullTerminatedString(header, 255, 14);
    const qint64 fileSize = sizeStr.trimmed().toLongLong(&ok);
    if (!ok || fileSize < 0) {
      return false;
    }
    outInfo.fileSize = fileSize;

    if (isV2) {
      outInfo.filePath = parseNullTerminatedString(header, 281, 4088);
      outInfo.crc32 = parseNullTerminatedString(header, 4369, 8);
    } else {
      outInfo.filePath = parseNullTerminatedString(header, 281, 4096);
    }

    return true;
  }

  static QString parseNullTerminatedString(const QByteArray& src, int offset, int length)
  {
    QByteArray bytes = src.mid(offset, length);
    const int nullPos = bytes.indexOf('\0');
    if (nullPos >= 0) {
      bytes = bytes.left(nullPos);
    }
    return QString::fromUtf8(bytes.trimmed());
  }

  static bool isEofBlock(const QByteArray& block)
  {
    if (block.size() != kHeaderSize) {
      return false;
    }
    if (isV2EofBlock(block)) {
      return true;
    }
    return block == QByteArray(kHeaderSize, '\0');
  }

  static bool isV2EofBlock(const QByteArray& block)
  {
    if (block.size() != kHeaderSize) {
      return false;
    }

    // v2 EOF: a255(null) + a14(size) + a4100(null) + a8(crc)
    // Filename must be empty (all null) to distinguish from file headers
    if (block.left(255) != QByteArray(255, '\0')) {
      return false;
    }

    const QString sizeField = parseNullTerminatedString(block, 255, 14);
    if (sizeField.isEmpty()) {
      return false;
    }

    const QByteArray crcField = block.mid(4369, 8);
    static const QRegularExpression hexPattern("^[0-9a-fA-F]{8}$");
    return hexPattern.match(QString::fromLatin1(crcField)).hasMatch();
  }

  bool verifyArchiveCrc()
  {
    const qint64 dataSize = size() - kHeaderSize;
    if (dataSize <= 0) {
      return true;
    }

    // Read EOF block to extract expected CRC (last 8 bytes)
    if (!seek(size() - kHeaderSize)) {
      return true;
    }
    const QByteArray eofBlock = QFile::read(kHeaderSize);
    const QString expectedCrc = QString::fromLatin1(eofBlock.mid(4369, 8));

    if (!seek(0)) {
      return true;
    }

    uLong crc = ::crc32(0L, Z_NULL, 0);
    qint64 remaining = dataSize;

    while (remaining > 0) {
      const qint64 toRead = qMin(remaining, kCrcChunkSize);
      const QByteArray chunk = QFile::read(toRead);
      if (chunk.isEmpty()) {
        break;
      }
      crc = ::crc32(crc, reinterpret_cast<const Bytef*>(chunk.constData()), chunk.size());
      remaining -= chunk.size();
    }

    const QString actualCrc = QString::asprintf("%08x", static_cast<unsigned int>(crc));
    if (actualCrc != expectedCrc) {
      emit error(QString("This backup file is damaged and can't be extracted.<br />Try downloading or transferring the file again.<br /><br /><b>Reason:</b> File integrity check failed (CRC mismatch). <a href=\"https://help.servmask.com/knowledgebase/import-failed-crc-mismatch/\">Technical details</a>"));
      return false;
    }

    seek(0);
    return true;
  }

  static QString computeFileCrc32(const QString& filePath)
  {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      return QString();
    }

    uLong crc = ::crc32(0L, Z_NULL, 0);

    while (!file.atEnd()) {
      const QByteArray chunk = file.read(kCrcChunkSize);
      crc = ::crc32(crc, reinterpret_cast<const Bytef*>(chunk.constData()), chunk.size());
    }

    file.close();
    return QString::asprintf("%08x", static_cast<unsigned int>(crc));
  }

  bool readExact(qint64 sizeToRead, QByteArray& out)
  {
    out = read(sizeToRead);
    return out.size() == sizeToRead;
  }

  static QString buildOutputPath(const QDir& extractTo, const QString& fileName, const QString& filePath)
  {
    if (filePath.isEmpty() || filePath == ".") {
      return extractTo.path() + "/" + fileName;
    }

    const QString dirPath = extractTo.path() + "/" + filePath;
    QDir dir(dirPath);
    if (!dir.exists()) {
      if (!QDir().mkpath(dirPath)) {
        return QString();
      }
    }
    return dirPath + "/" + fileName;
  }

private:
  qint64 bytesRead;
  QString filePassword;
  bool isEncrypted;
  bool isV2;
  CompressionType compressionType;
};

#endif // BACKUPFILE_H
