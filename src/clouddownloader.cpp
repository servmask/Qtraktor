#include "clouddownloader.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QUrlQuery>
#include <openssl/evp.h>

CloudDownloader::CloudDownloader(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

CloudDownloader::~CloudDownloader()
{
    abort();
    cleanupAes();
    cleanupTempFile();
}

// ── URL helpers ────────────────────────────────────────────────────────────────

bool CloudDownloader::isRemoteUrl(const QUrl &url)
{
    // Qt 6 removed FTP support from QNetworkAccessManager; only HTTP(S) accepted.
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

QString CloudDownloader::extractGoogleDriveId(const QUrl &url)
{
    // https://drive.google.com/file/d/{ID}/view
    static const QRegularExpression pathId(QStringLiteral("/file/d/([^/]+)"));
    const QRegularExpressionMatch m = pathId.match(url.path());
    if (m.hasMatch())
        return m.captured(1);

    // https://drive.google.com/open?id={ID}
    return QUrlQuery(url).queryItemValue(QStringLiteral("id"));
}

// Parses mega.nz file URLs and derives the AES-128-CTR key + IV from the
// URL fragment. The fragment is NEVER sent to any server.
//
// Supported URL formats:
//   https://mega.nz/file/{ID}#{KEY}   (current)
//   https://mega.nz/#!{ID}!{KEY}      (legacy)
//
// The 32-byte base64url KEY is stored as 8 x uint32 words:
//   words[0..3]  XOR'd key material  (bytes  0-15)
//   words[4..5]  IV / CTR nonce      (bytes 16-23)
//   words[6..7]  meta-MAC            (bytes 24-31)
//
// Real AES-128 key = bytes[0..15] XOR bytes[16..31]
// CTR nonce       = bytes[16..23]
CloudDownloader::MegaCtx CloudDownloader::parseMegaUrl(const QUrl &url)
{
    MegaCtx ctx;
    const QString host = url.host().toLower();
    if (host != QLatin1String("mega.nz") && host != QLatin1String("mega.co.nz"))
        return ctx;

    QString keyStr;
    const QString path = url.path();
    const QString fragment = url.fragment(QUrl::FullyEncoded);

    if (path.startsWith(QLatin1String("/file/"))) {
        // New format: /file/{ID}#{KEY}
        keyStr = fragment;
    } else if (fragment.startsWith(QLatin1Char('!'))) {
        // Legacy format: #!{ID}!{KEY}
        const int sep = fragment.indexOf(QLatin1Char('!'), 1);
        if (sep < 0)
            return ctx;
        keyStr = fragment.mid(sep + 1);
    } else {
        return ctx;
    }

    if (keyStr.isEmpty())
        return ctx;

    // Mega uses standard base64url (- and _ instead of + and /, no padding).
    const QByteArray keyBytes =
        QByteArray::fromBase64(keyStr.toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    if (keyBytes.size() < 32)
        return ctx;

    // Derive real AES key: XOR both 16-byte halves word by word.
    QByteArray aesKey(16, '\0');
    for (int i = 0; i < 16; ++i)
        aesKey[i] =
            static_cast<char>(static_cast<unsigned char>(keyBytes[i]) ^ static_cast<unsigned char>(keyBytes[i + 16]));

    ctx.aesKey = aesKey;
    ctx.iv = keyBytes.mid(16, 8); // CTR nonce = words[4..5]
    ctx.valid = true;
    return ctx;
}

QUrl CloudDownloader::normalizeUrl(const QUrl &url)
{
    const QString host = url.host().toLower();

    // ── Google Drive ───────────────────────────────────────────────────────
    if (host == QLatin1String("drive.google.com")) {
        const QString id = extractGoogleDriveId(url);
        if (!id.isEmpty()) {
            QUrl dl(QStringLiteral("https://drive.google.com/uc"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("export"), QStringLiteral("download"));
            q.addQueryItem(QStringLiteral("id"), id);
            q.addQueryItem(QStringLiteral("confirm"), QStringLiteral("t"));
            dl.setQuery(q);
            return dl;
        }
    }

    // ── Dropbox ────────────────────────────────────────────────────────────
    if (host == QLatin1String("www.dropbox.com") || host == QLatin1String("dropbox.com")) {
        QUrlQuery q(url);
        q.removeQueryItem(QStringLiteral("dl"));
        q.addQueryItem(QStringLiteral("dl"), QStringLiteral("1"));
        QUrl dl = url;
        dl.setQuery(q);
        return dl;
    }

    // ── OneDrive / 1drv.ms short links ────────────────────────────────────
    if (host == QLatin1String("1drv.ms") || host.endsWith(QLatin1String(".1drv.ms"))) {
        QUrlQuery q(url);
        q.removeQueryItem(QStringLiteral("download"));
        q.addQueryItem(QStringLiteral("download"), QStringLiteral("1"));
        QUrl dl = url;
        dl.setQuery(q);
        return dl;
    }
    if (host == QLatin1String("onedrive.live.com")) {
        QUrlQuery q(url);
        q.removeQueryItem(QStringLiteral("download"));
        q.addQueryItem(QStringLiteral("download"), QStringLiteral("1"));
        QUrl dl = url;
        dl.setQuery(q);
        return dl;
    }

    // ── Box ────────────────────────────────────────────────────────────────
    if (host == QLatin1String("app.box.com")) {
        static const QRegularExpression boxShare(QStringLiteral("^/s/([^/]+)$"));
        const QRegularExpressionMatch m = boxShare.match(url.path());
        if (m.hasMatch())
            return QUrl(QStringLiteral("https://app.box.com/shared/static/") + m.captured(1));
    }

    // ── pCloud ────────────────────────────────────────────────────────────
    if (host == QLatin1String("u.pcloud.link")) {
        const QString code = QUrlQuery(url).queryItemValue(QStringLiteral("code"));
        if (!code.isEmpty()) {
            QUrl dl(QStringLiteral("https://u.pcloud.link/publink/code"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("code"), code);
            q.addQueryItem(QStringLiteral("forcedownload"), QStringLiteral("1"));
            dl.setQuery(q);
            return dl;
        }
    }

    // Amazon S3, DigitalOcean Spaces, Google Cloud Storage, Azure Blob —
    // direct / pre-signed HTTPS URLs work as-is.
    // Mega is handled inside download(), not here (needs API call + decryption).
    return url;
}

// ── Cleanup helpers ───────────────────────────────────────────────────────────

void CloudDownloader::cleanupAes()
{
    if (m_aesCtr) {
        EVP_CIPHER_CTX_free(m_aesCtr);
        m_aesCtr = nullptr;
    }
}

void CloudDownloader::cleanupTempFile()
{
    if (m_tempFile) {
        if (m_tempFile->exists())
            QFile::remove(m_tempFile->fileName());
        delete m_tempFile;
        m_tempFile = nullptr;
    }
}

// ── Download ───────────────────────────────────────────────────────────────────

void CloudDownloader::download(const QUrl &rawUrl)
{
    if (m_reply || m_megaApiReply)
        return;

    m_aborted = false;
    m_isMega = false;
    m_decryptFailed = false;
    cleanupAes();
    cleanupTempFile(); // remove any leftover temp file from a previous download

    m_suggestedName = QFileInfo(rawUrl.path()).fileName();
    if (m_suggestedName.isEmpty())
        m_suggestedName = QStringLiteral("backup.wpress");
    if (!m_suggestedName.endsWith(QLatin1String(".wpress"), Qt::CaseInsensitive))
        m_suggestedName += QStringLiteral(".wpress");

    const QString host = rawUrl.host().toLower();
    if (host == QLatin1String("mega.nz") || host == QLatin1String("mega.co.nz")) {
        downloadMega(rawUrl);
        return;
    }

    startCdnDownload(normalizeUrl(rawUrl).toString());
}

// Step 1 (Mega): parse URL, POST to Mega API to obtain the CDN download URL.
// Only the public file node ID is sent to Mega's server; the decryption key
// stays in m_megaCtx and is never transmitted.
void CloudDownloader::downloadMega(const QUrl &url)
{
    m_megaCtx = parseMegaUrl(url);
    if (!m_megaCtx.valid) {
        emit failed(tr("Could not parse Mega URL. Make sure it is a file link "
                       "(mega.nz/file/\xe2\x80\xa6 or mega.nz/#!\xe2\x80\xa6) and includes "
                       "the decryption key after the # character."));
        return;
    }

    // Extract file ID from path (new format) or fragment (legacy format).
    QString fileId;
    const QString path = url.path();
    const QString fragment = url.fragment(QUrl::FullyEncoded);
    if (path.startsWith(QLatin1String("/file/"))) {
        fileId = path.mid(6);
    } else {
        const QString inner = fragment.mid(1); // strip leading !
        fileId = inner.left(inner.indexOf(QLatin1Char('!')));
    }

    const QByteArray body =
        QJsonDocument(QJsonArray({QJsonObject({{"a", "g"}, {"g", 1}, {"p", fileId}})})).toJson(QJsonDocument::Compact);

    QNetworkRequest req(QUrl(QStringLiteral("https://g.api.mega.co.nz/cs?id=1&domain=meganz")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));

    m_isMega = true;
    m_megaApiReply = m_nam->post(req, body);
    connect(m_megaApiReply, &QNetworkReply::finished, this, &CloudDownloader::onMegaApiFinished);
}

// Step 2 (Mega): parse the API response and start the actual CDN download.
void CloudDownloader::onMegaApiFinished()
{
    if (m_aborted) {
        m_megaApiReply->deleteLater();
        m_megaApiReply = nullptr;
        return;
    }

    const QByteArray response = m_megaApiReply->readAll();
    const QNetworkReply::NetworkError err = m_megaApiReply->error();
    const QString errStr = m_megaApiReply->errorString();
    m_megaApiReply->deleteLater();
    m_megaApiReply = nullptr;

    if (err != QNetworkReply::NoError) {
        emit failed(tr("Mega API request failed: %1").arg(errStr));
        return;
    }

    // Response is a JSON array: [{"g":"https://cdn...","s":123,"at":"..."}]
    // On error Mega returns a negative integer, e.g. [-9].
    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isArray() || doc.array().isEmpty()) {
        emit failed(tr("Unexpected response from Mega API."));
        return;
    }

    const QJsonValue first = doc.array().first();

    if (first.isDouble() && first.toInt() < 0) {
        const int code = first.toInt();
        QString reason;
        switch (code) {
        case -9:
            reason = tr("file not found (it may have been deleted or made private)");
            break;
        case -11:
            reason = tr("download quota exceeded");
            break;
        default:
            reason = tr("error code %1").arg(code);
            break;
        }
        emit failed(tr("Mega returned an error: %1.").arg(reason));
        return;
    }

    if (!first.isObject()) {
        emit failed(tr("Could not read Mega API response."));
        return;
    }

    const QJsonObject obj = first.toObject();
    const QJsonValue g = obj.value(QLatin1String("g"));

    QString cdnUrl;
    if (g.isString()) {
        cdnUrl = g.toString();
    } else if (g.isArray()) {
        // Multi-part downloads (very large files) require reassembling segments
        // in order. Silently using only the first segment would produce a corrupt
        // file; fail explicitly so the user knows to download manually instead.
        emit failed(tr("This Mega file uses multi-part download (file is very large). "
                       "Please download it manually and drag the local file into Traktor."));
        return;
    }

    if (cdnUrl.isEmpty()) {
        emit failed(tr("Mega API response did not include a download URL."));
        return;
    }

    // Initialize streaming AES-128-CTR decryption.
    // Counter block: IV (8 bytes) || 0x00 * 8
    unsigned char ivBlock[16] = {};
    Q_ASSERT(m_megaCtx.iv.size() == 8);
    memcpy(ivBlock, m_megaCtx.iv.constData(), 8);

    m_aesCtr = EVP_CIPHER_CTX_new();
    if (!m_aesCtr ||
        EVP_DecryptInit_ex(m_aesCtr, EVP_aes_128_ctr(), nullptr,
                           reinterpret_cast<const unsigned char *>(m_megaCtx.aesKey.constData()), ivBlock) != 1) {
        cleanupAes();
        emit failed(tr("Failed to initialize AES decryption context."));
        return;
    }

    startCdnDownload(cdnUrl);
}

// Common path: open temp file and issue the GET for the CDN URL.
void CloudDownloader::startCdnDownload(const QString &cdnUrl)
{
    m_tempFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/traktor-XXXXXX.wpress"), this);
    m_tempFile->setAutoRemove(false); // MainWindow takes ownership on success
    if (!m_tempFile->open()) {
        const QString err = m_tempFile->errorString();
        cleanupTempFile();
        emit failed(tr("Could not create temporary file: %1").arg(err));
        return;
    }

    const QUrl cdnQUrl(cdnUrl);
    QNetworkRequest req(cdnQUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &CloudDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &CloudDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &CloudDownloader::onFinished);
}

void CloudDownloader::abort()
{
    m_aborted = true;
    if (m_megaApiReply) {
        m_megaApiReply->abort();
        m_megaApiReply->deleteLater();
        m_megaApiReply = nullptr;
    }
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    cleanupAes();
    cleanupTempFile(); // remove partial download from disk
}

// ── Private slots ─────────────────────────────────────────────────────────────

void CloudDownloader::onReadyRead()
{
    if (!m_tempFile || !m_reply || m_decryptFailed)
        return;

    const QByteArray chunk = m_reply->readAll();
    if (chunk.isEmpty())
        return;

    if (m_isMega && m_aesCtr) {
        QByteArray plain(chunk.size() + EVP_MAX_BLOCK_LENGTH, '\0');
        int outLen = 0;
        if (EVP_DecryptUpdate(m_aesCtr, reinterpret_cast<unsigned char *>(plain.data()), &outLen,
                              reinterpret_cast<const unsigned char *>(chunk.constData()),
                              static_cast<int>(chunk.size())) != 1) {
            m_decryptFailed = true;
            return; // onFinished() will emit failed() and clean up
        }
        plain.resize(outLen);
        m_tempFile->write(plain);
    } else {
        m_tempFile->write(chunk);
    }
}

void CloudDownloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
        emit progress(static_cast<int>(received * 100 / total));
    else
        emit progress(-1);
}

void CloudDownloader::onFinished()
{
    if (m_aborted) {
        m_reply->deleteLater();
        m_reply = nullptr;
        cleanupAes();
        // cleanupTempFile() already called in abort()
        return;
    }

    // Flush any remaining buffered data (before checking for errors)
    if (m_tempFile && m_reply && !m_decryptFailed)
        m_tempFile->write(m_reply->readAll());

    if (m_decryptFailed) {
        m_reply->deleteLater();
        m_reply = nullptr;
        cleanupAes();
        cleanupTempFile();
        emit failed(tr("AES-128-CTR decryption failed mid-stream. "
                       "The Mega link may be corrupt or the key is incorrect."));
        return;
    }

    // Finalize CTR context (no padding in CTR mode, but required by OpenSSL API)
    if (m_isMega && m_aesCtr) {
        unsigned char finalBuf[EVP_MAX_BLOCK_LENGTH];
        int finalLen = 0;
        EVP_DecryptFinal_ex(m_aesCtr, finalBuf, &finalLen);
        if (m_tempFile && finalLen > 0)
            m_tempFile->write(reinterpret_cast<const char *>(finalBuf), finalLen);
        cleanupAes();
    }

    const QNetworkReply::NetworkError err = m_reply->error();
    const QString errStr = m_reply->errorString();
    const int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    m_reply->deleteLater();
    m_reply = nullptr;

    if (err != QNetworkReply::NoError) {
        cleanupTempFile();
        emit failed(tr("Download failed: %1").arg(errStr));
        return;
    }

    if (httpStatus != 0 && (httpStatus < 200 || httpStatus >= 300)) {
        cleanupTempFile();
        emit failed(tr("Server returned HTTP %1. Make sure the file is publicly "
                       "shared or use a direct download link.")
                        .arg(httpStatus));
        return;
    }

    if (!m_tempFile) {
        emit failed(tr("Internal error: temporary file lost during download."));
        return;
    }

    m_tempFile->flush();
    m_tempFile->close();

    // Sanity check: detect HTML interstitials (e.g. Google Drive virus-scan
    // warning pages) that get served instead of the actual file. A .wpress
    // file never starts with '<'.
    {
        QFile probe(m_tempFile->fileName());
        if (probe.open(QIODevice::ReadOnly)) {
            const QByteArray head = probe.read(16);
            probe.close();
            if (!head.isEmpty() && head.trimmed().startsWith('<')) {
                cleanupTempFile();
                emit failed(tr("The downloaded content is not a .wpress file (received HTML). "
                               "The provider may require authentication or a direct download link. "
                               "For Google Drive, try opening the link in a browser first to "
                               "dismiss any virus-scan warning."));
                return;
            }
        }
    }

    emit finished(m_tempFile->fileName(), m_suggestedName);
}
