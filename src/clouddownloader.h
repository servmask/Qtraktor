#ifndef CLOUDDOWNLOADER_H
#define CLOUDDOWNLOADER_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

struct mega_aes_ctr; // opaque AES-128-CTR context (src/megaaes.h)

// Downloads a .wpress file from a remote URL to a temp file, emitting progress
// along the way. Supports direct HTTP(S) URLs and normalizes share links from
// common cloud storage providers into direct download URLs.
//
// Mega (mega.nz) is handled specially: the file ID and AES-128-CTR key are
// extracted from the URL fragment, the Mega API is called to get a CDN URL,
// and the encrypted stream is decrypted on-the-fly before writing to the temp
// file. The decryption key never leaves the client.
class CloudDownloader : public QObject
{
    Q_OBJECT
public:
    // Parsed result of a mega.nz file URL. Public so unit tests can verify it.
    struct MegaCtx {
        QByteArray aesKey; // 16 bytes: XOR of bytes[0..15] and bytes[16..31] of the link key
        QByteArray iv;     // 8 bytes: bytes[16..23] of the link key (CTR nonce)
        bool valid = false;
    };

    explicit CloudDownloader(QObject *parent = nullptr);
    ~CloudDownloader();

    // Returns true if url is a remote HTTP(S) URL worth downloading.
    // Note: Qt 6 removed FTP support from QNetworkAccessManager; ftp:// is not accepted.
    static bool isRemoteUrl(const QUrl &url);

    // Converts a cloud share/view URL into a direct download URL where possible.
    // Mega URLs are NOT normalized here — they are handled inside download().
    // If no conversion is known the original URL is returned unchanged.
    static QUrl normalizeUrl(const QUrl &url);

    // Parses a mega.nz file URL and extracts the AES-128-CTR key and IV from
    // the URL fragment. Public so tests can verify key derivation independently.
    static MegaCtx parseMegaUrl(const QUrl &url);

    // Begin downloading url. Emits progress(), then either finished() or failed().
    void download(const QUrl &url);

    // Cancel an in-flight download. failed() will NOT be emitted after abort().
    void abort();

signals:
    // 0-100 percent; -1 when Content-Length is unknown (use a busy indicator).
    void progress(int percent);
    // tempFilePath is valid until the next download() or CloudDownloader destruction.
    void finished(const QString &tempFilePath, const QString &suggestedName);
    void failed(const QString &errorMessage);

private slots:
    void onReadyRead();
    void onFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onMegaApiFinished();

private:
    // ── generic download state ─────────────────────────────────────────────
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QTemporaryFile *m_tempFile = nullptr;
    QString m_suggestedName;
    bool m_aborted = false;

    // ── Mega-specific state ────────────────────────────────────────────────
    bool m_isMega = false;
    MegaCtx m_megaCtx;
    QNetworkReply *m_megaApiReply = nullptr;
    mega_aes_ctr *m_aesCtr = nullptr; // AES-128-CTR context (null unless Mega)

    // ── helpers ────────────────────────────────────────────────────────────
    static QString extractGoogleDriveId(const QUrl &url);
    void downloadMega(const QUrl &url);
    void startCdnDownload(const QString &cdnUrl);
    void writeChunk(QByteArray data); // AES-128-CTR decrypt in place (if Mega), then write
    void cleanupAes();
    void cleanupTempFile(); // removes temp file from disk and frees the object
};

#endif // CLOUDDOWNLOADER_H
