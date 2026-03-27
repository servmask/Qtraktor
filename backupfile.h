#ifndef BACKUPFILE_H
#define BACKUPFILE_H

#include <QAtomicInt>
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
      compressionType(COMPRESSION_NONE),
      abortFlag(nullptr)
  {}

  bool isEncryptedFile() const { return isEncrypted; }
  bool isV2Format() const { return isV2; }
  CompressionType getCompressionType() const { return compressionType; }

  void setAbortFlag(QAtomicInt *flag) { abortFlag = flag; }

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

  bool verifyArchiveCrc();

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
      if (abortFlag && abortFlag->loadAcquire() != 0) {
        log("Extraction cancelled.", true);
        return false;
      }

      HeaderInfo info;
      if (!readHeader(info)) {
        log("Failed to read file header. The backup file might be corrupted or truncated.", true);
        return false;
      }

      if (info.isEof) {
        return true;
      }

      const QString fullPath = buildOutputPath(extractTo, info.fileName, info.filePath);

      QFile out(fullPath);
      if (!out.open(QIODevice::WriteOnly)) {
        log(QString("Error: Failed to create output file: %1 (Permissions?). Skipping.").arg(fullPath), true);
        // Skip over the content bytes we won't process
        if (!seek(pos() + info.fileSize)) {
          log(QString("Fatal: Failed to skip content for file: %1.").arg(info.fileName), true);
          return false;
        }
        continue;
      }

      const bool isCompressed = !CryptoUtils::isConfigFile(info.fileName) && compressionType != COMPRESSION_NONE;

      QString processError;
      bool streamOk;

      if (isEncrypted && !filePassword.isEmpty()) {
        streamOk = CryptoUtils::processFileContentWithPasswordStreaming(
          this, info.fileSize, &out, isCompressed, info.fileName, filePassword, compressionType, &processError
        );
      } else {
        streamOk = CryptoUtils::processFileContentStreaming(
          this, info.fileSize, &out, isCompressed, info.fileName, compressionType, &processError
        );
      }

      out.close();

      if (!streamOk) {
        if (abortFlag && abortFlag->loadAcquire() != 0) {
          log("Extraction cancelled.", true);
          return false;
        }
        log(QString("Error processing file '%1': %2. Skipping.").arg(info.fileName, processError), true);
        continue;
      }

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
    if (abortFlag && abortFlag->loadAcquire() != 0)
      return -1;

    const qint64 n = QFile::readData(data, maxlen);
    bytesRead += n;

    if (size() > 0) {
      emit progress((static_cast<float>(bytesRead) / static_cast<float>(size())) * 100.0f);
    } else {
      emit progress(0.0f);
    }

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
  QAtomicInt *abortFlag;
};

#endif // BACKUPFILE_H
