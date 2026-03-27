#include "backupfile.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <zlib.h>

namespace {
  struct FileStateGuard {
    BackupFile* f = nullptr;
    qint64 savedPos = 0;
    bool shouldClose = false;

    FileStateGuard(BackupFile* file, qint64 pos, bool closeOnExit) : f(file), savedPos(pos), shouldClose(closeOnExit) {}

    ~FileStateGuard() {
      if (!f) return;
      f->seek(savedPos);
      if (shouldClose) f->close();
    }
  };

  static bool isConfigFileNameMatch(const QString& fileName)
  {
    const QString baseFileName = fileName.section('/', -1);
    const QString normalized = fileName.trimmed();

    return (baseFileName == "package.json" || baseFileName == "multisite.json" ||
            normalized == "package.json" || normalized == "multisite.json" ||
            normalized.endsWith("/package.json") || normalized.endsWith("/multisite.json"));
  }

  static bool jsonBoolish(const QJsonValue& v, bool defaultValue = false)
  {
    if (v.isBool()) return v.toBool();
    if (v.isString()) {
      const QString s = v.toString().toLower();
      return (s == "true" || s == "1");
    }
    return defaultValue;
  }
}

void BackupFile::loadConfig()
{
  isEncrypted = false;
  compressionType = COMPRESSION_NONE;

  const bool wasOpen = isOpen();
  if (!wasOpen) {
    if (!open(QIODevice::ReadOnly)) {
      return;
    }
  }

  const qint64 savedPos = pos();
  FileStateGuard guard(this, savedPos, !wasOpen);

  if (!seek(0)) {
    return;
  }

  while (!atEnd()) {
    HeaderInfo info;
    if (!readHeader(info)) {
      break;
    }

    if (info.isEof) {
      break;
    }

    if (isConfigFileNameMatch(info.fileName)) {
      QByteArray jsonContent;
      if (!readExact(info.fileSize, jsonContent)) {
        break;
      }

      QJsonParseError err;
      const QJsonDocument doc = QJsonDocument::fromJson(jsonContent, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();

        if (obj.contains("Encrypted")) {
          isEncrypted = jsonBoolish(obj.value("Encrypted"), false);
        }

        if (obj.contains("Compression") && obj.value("Compression").isObject()) {
          const QJsonObject compression = obj.value("Compression").toObject();
          const bool enabled = compression.contains("Enabled") ? jsonBoolish(compression.value("Enabled"), false) : false;

          if (enabled && compression.contains("Type")) {
            const QString type = compression.value("Type").toString().toLower();

            if (type == "zlib" || type == "gzip") {
              compressionType = COMPRESSION_ZLIB;
            } else if (type == "bzip2") {
              compressionType = COMPRESSION_BZIP2;
            }
          }
        }
      }

      break;
    }

    if (!seek(pos() + info.fileSize)) {
      break;
    }
  }
}

bool BackupFile::verifyArchiveCrc()
{
  const qint64 dataSize = size() - kHeaderSize;
  if (dataSize <= 0) {
    return true;
  }

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
    if (abortFlag && abortFlag->loadAcquire() != 0)
      return false;

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
    emit error(QString("This backup file is damaged and can't be extracted.<br />"
                        "Try downloading or transferring the file again.<br /><br />"
                        "<b>Reason:</b> File integrity check failed (CRC mismatch). "
                        "<a href=\"https://help.servmask.com/knowledgebase/import-failed-crc-mismatch/\">"
                        "Technical details</a>"));
    return false;
  }

  seek(0);
  return true;
}
