#include "backupfile.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <zlib.h>

namespace
{
struct FileStateGuard {
    BackupFile *f = nullptr;
    qint64 savedPos = 0;
    bool shouldClose = false;

    FileStateGuard(BackupFile *file, qint64 pos, bool closeOnExit) : f(file), savedPos(pos), shouldClose(closeOnExit) {}

    ~FileStateGuard()
    {
        if (!f)
            return;
        f->seek(savedPos);
        if (shouldClose)
            f->close();
    }
};

static bool isConfigFileNameMatch(const QString &fileName)
{
    const QString baseFileName = fileName.section('/', -1);
    const QString normalized = fileName.trimmed();

    return (baseFileName == "package.json" || baseFileName == "multisite.json" || normalized == "package.json" ||
            normalized == "multisite.json" || normalized.endsWith("/package.json") ||
            normalized.endsWith("/multisite.json"));
}

static bool jsonBoolish(const QJsonValue &v, bool defaultValue = false)
{
    if (v.isBool())
        return v.toBool();
    if (v.isString()) {
        const QString s = v.toString().toLower();
        return (s == "true" || s == "1");
    }
    return defaultValue;
}
} // namespace

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
                    const bool enabled =
                        compression.contains("Enabled") ? jsonBoolish(compression.value("Enabled"), false) : false;

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

QString BackupFile::normalizePath(const QString &filePath, const QString &fileName)
{
    QString path;
    if (filePath.isEmpty() || filePath == ".") {
        path = fileName;
    } else {
        path = filePath + "/" + fileName;
    }
    // Strip leading "./"
    while (path.startsWith("./")) {
        path = path.mid(2);
    }
    // Collapse double slashes
    while (path.contains("//")) {
        path.replace("//", "/");
    }
    return path;
}

bool BackupFile::iterateHeaders(std::function<bool(const HeaderInfo &)> callback)
{
    const qint64 savedPos = pos();

    if (!seek(0)) {
        return false;
    }

    ensureConfigLoaded();

    // Skip to first file entry (re-seek after config load)
    if (!seek(0)) {
        seek(savedPos);
        return false;
    }

    while (!atEnd()) {
        HeaderInfo info;
        if (!readHeader(info)) {
            seek(savedPos);
            return false;
        }

        if (info.isEof) {
            seek(savedPos);
            return true;
        }

        const qint64 posBeforeCallback = pos();

        if (!callback(info)) {
            seek(savedPos);
            return true; // callback requested stop, not an error
        }

        // Skip past file content ONLY if the callback didn't already consume it.
        // Callbacks like verify stream through the content via CryptoUtils,
        // which advances the file pointer. We detect this by checking whether
        // the position moved since before the callback.
        const qint64 expectedEnd = posBeforeCallback + info.fileSize;
        if (pos() < expectedEnd) {
            if (!seek(expectedEnd)) {
                seek(savedPos);
                return false;
            }
        }
    }

    seek(savedPos);
    return false; // unexpected end of file
}

bool BackupFile::extractSingleFile(const QString &targetPath, QIODevice *dest)
{
    ensureConfigLoaded();

    if (!seek(0)) {
        return false;
    }

    const QString normalizedTarget = normalizePath("", targetPath);

    while (!atEnd()) {
        HeaderInfo info;
        if (!readHeader(info)) {
            return false;
        }

        if (info.isEof) {
            return false; // file not found
        }

        const QString entryPath = normalizePath(info.filePath, info.fileName);

        if (entryPath == normalizedTarget) {
            // Found the target file — stream it to dest
            const bool isCompressed = !CryptoUtils::isConfigFile(info.fileName) && compressionType != COMPRESSION_NONE;

            QString processError;
            bool streamOk;

            if (isEncrypted && !filePassword.isEmpty()) {
                streamOk = CryptoUtils::processFileContentWithPasswordStreaming(this, info.fileSize, dest, isCompressed,
                                                                                info.fileName, filePassword,
                                                                                compressionType, &processError);
            } else {
                streamOk = CryptoUtils::processFileContentStreaming(this, info.fileSize, dest, isCompressed,
                                                                    info.fileName, compressionType, &processError);
            }

            if (!streamOk) {
                emit error(QString("Error processing file '%1': %2").arg(info.fileName, processError));
            }
            return streamOk;
        }

        // Skip past non-matching file content
        if (!seek(pos() + info.fileSize)) {
            return false;
        }
    }

    return false; // file not found
}

QJsonObject BackupFile::getArchiveInfo()
{
    ensureConfigLoaded();

    QJsonObject info;
    info["version"] = isV2 ? 2 : 1;
    info["encrypted"] = isEncrypted;

    switch (compressionType) {
    case COMPRESSION_ZLIB:
        info["compression"] = QString("zlib");
        break;
    case COMPRESSION_BZIP2:
        info["compression"] = QString("bzip2");
        break;
    default:
        info["compression"] = QString("none");
        break;
    }

    info["archiveSize"] = size();

    // Full header scan for file count and total size
    int totalFiles = 0;
    qint64 totalSize = 0;

    const bool scanOk = iterateHeaders([&](const HeaderInfo &entry) {
        totalFiles++;
        totalSize += entry.fileSize;
        return true;
    });

    info["totalFiles"] = totalFiles;
    info["totalSize"] = totalSize;
    info["scanComplete"] = scanOk;

    return info;
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
        crc = ::crc32(crc, reinterpret_cast<const Bytef *>(chunk.constData()), static_cast<uInt>(chunk.size()));
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
