#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>

enum CompressionType { COMPRESSION_NONE = 0, COMPRESSION_ZLIB = 1, COMPRESSION_BZIP2 = 2 };

class CryptoUtils
{

public:
    static bool isConfigFile(const QString &fileName);

    static QByteArray decompressZlibChunk(const QByteArray &compressedData, QString *errorMsg = nullptr);

    static QByteArray decompressBzip2Chunk(const QByteArray &compressedData, QString *errorMsg = nullptr);

    static QByteArray decompressChunk(const QByteArray &compressedData, CompressionType type,
                                      QString *errorMsg = nullptr);

    static QByteArray processFileContent(const QByteArray &fileContent, bool isCompressed, const QString &fileName,
                                         CompressionType compressionType = COMPRESSION_NONE,
                                         QString *errorMsg = nullptr);

    static QByteArray decryptString(const QByteArray &encryptedData, const QString &password,
                                    QString *errorMsg = nullptr);

    static QByteArray processFileContentWithPassword(const QByteArray &fileContent, bool isCompressed,
                                                     const QString &fileName, const QString &password,
                                                     CompressionType compressionType = COMPRESSION_NONE,
                                                     QString *errorMsg = nullptr);

    static int cryptIvLength();

    // Streaming variants: read from source QIODevice, write to dest QIODevice.
    // Processes contentSize bytes from current source position.
    // Returns true on success, false on error (with errorMsg set).
    static bool processFileContentStreaming(QIODevice *source, qint64 contentSize, QIODevice *dest, bool isCompressed,
                                            const QString &fileName, CompressionType compressionType = COMPRESSION_NONE,
                                            QString *errorMsg = nullptr);

    static bool processFileContentWithPasswordStreaming(QIODevice *source, qint64 contentSize, QIODevice *dest,
                                                        bool isCompressed, const QString &fileName,
                                                        const QString &password,
                                                        CompressionType compressionType = COMPRESSION_NONE,
                                                        QString *errorMsg = nullptr);

private:
    static const QStringList CONFIG_FILES;
    static const int CHUNK_SIZE_PREFIX_LENGTH; // 4 bytes for compressed chunk size
    static const int ENCRYPTION_CHUNK_SIZE;    // 512000 + 16 (IV) + 16 (AES padding)
    static const int STREAM_COPY_CHUNK_SIZE;   // 512KB for plain file streaming

    static bool readExactFromDevice(QIODevice *source, qint64 size, QByteArray &out);
    static bool copyPlain(QIODevice *source, qint64 contentSize, QIODevice *dest, QString *errorMsg);
    static bool streamCompressed(QIODevice *source, qint64 contentSize, QIODevice *dest, CompressionType type,
                                 QString *errorMsg);
    static bool streamEncryptedOnly(QIODevice *source, qint64 contentSize, QIODevice *dest, const QString &password,
                                    QString *errorMsg);
    static bool streamCompressedEncrypted(QIODevice *source, qint64 contentSize, QIODevice *dest,
                                          const QString &password, CompressionType type, QString *errorMsg);
};

#endif // CRYPTOUTILS_H
