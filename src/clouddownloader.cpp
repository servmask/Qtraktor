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
#include <chrono>
#include <utility>
#include "megaaes.h"

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
    // HTTPS only: downloads must be transport-encrypted so a network attacker
    // cannot tamper with the backup bytes. (Qt 6 also removed FTP from
    // QNetworkAccessManager, and plain http:// is intentionally rejected.)
    return url.scheme().toLower() == QLatin1String("https");
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
    // Google deprecated the drive.google.com/uc endpoint (returns 403).
    // The current working endpoint is drive.usercontent.google.com/download.
    if (host == QLatin1String("drive.google.com")) {
        const QString id = extractGoogleDriveId(url);
        if (!id.isEmpty()) {
            QUrl dl(QStringLiteral("https://drive.usercontent.google.com/download"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("id"), id);
            q.addQueryItem(QStringLiteral("export"), QStringLiteral("download"));
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

    // pCloud is handled inside download() (needs API call to resolve CDN URL).
    // DigitalOcean Spaces and other direct / pre-signed HTTPS URLs work as-is.
    // Mega is handled inside download(), not here (needs API call + decryption).
    return url;
}

// ── Cleanup helpers ───────────────────────────────────────────────────────────

void CloudDownloader::cleanupAes()
{
    if (m_aesCtr) {
        mega_aes_ctr_free(m_aesCtr);
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
    if (m_reply || m_megaApiReply || m_pcloudApiReply || m_wetransferReply)
        return;

    m_aborted = false;
    m_writeFailed = false;
    m_isMega = false;
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

    if (host == QLatin1String("pcloud.link") || host.endsWith(QLatin1String(".pcloud.link")) ||
        host == QLatin1String("my.pcloud.com")) {
        downloadPCloud(rawUrl);
        return;
    }

    if (host == QLatin1String("we.tl") || host == QLatin1String("wetransfer.com") ||
        host == QLatin1String("www.wetransfer.com")) {
        downloadWeTransfer(rawUrl);
        return;
    }

    startCdnDownload(normalizeUrl(rawUrl).toString());
}

// pCloud download: call the public API to resolve a CDN download URL from
// the share code, then hand off to startCdnDownload().
void CloudDownloader::downloadPCloud(const QUrl &url)
{
    const QString code = QUrlQuery(url).queryItemValue(QStringLiteral("code"));
    if (code.isEmpty()) {
        emit failed(tr("Could not extract the share code from this pCloud URL. "
                       "Make sure the link contains a \"code\" parameter."));
        return;
    }

    QUrl apiUrl(QStringLiteral("https://api.pcloud.com/getpublinkdownload"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("code"), code);
    apiUrl.setQuery(q);

    QNetworkRequest req(apiUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));

    req.setTransferTimeout(std::chrono::seconds(30));
    m_pcloudApiReply = m_nam->get(req);
    connect(m_pcloudApiReply, &QNetworkReply::finished, this, &CloudDownloader::onPCloudApiFinished);
}

void CloudDownloader::onPCloudApiFinished()
{
    if (m_aborted) {
        m_pcloudApiReply->deleteLater();
        m_pcloudApiReply = nullptr;
        return;
    }

    const QByteArray response = m_pcloudApiReply->readAll();
    const QNetworkReply::NetworkError err = m_pcloudApiReply->error();
    const QString errStr = m_pcloudApiReply->errorString();
    m_pcloudApiReply->deleteLater();
    m_pcloudApiReply = nullptr;

    if (err != QNetworkReply::NoError) {
        emit failed(tr("pCloud API request failed: %1").arg(errStr));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        emit failed(tr("Unexpected response from pCloud API."));
        return;
    }

    const QJsonObject obj = doc.object();
    const int result = obj.value(QLatin1String("result")).toInt(-1);

    if (result != 0) {
        QString reason;
        switch (result) {
        case 7002:
            reason = tr("file not found or the share link has expired");
            break;
        case 7003:
            reason = tr("file has been removed");
            break;
        default:
            reason = tr("error code %1").arg(result);
            break;
        }
        emit failed(tr("pCloud returned an error: %1.").arg(reason));
        return;
    }

    const QJsonArray hosts = obj.value(QLatin1String("hosts")).toArray();
    const QString path = obj.value(QLatin1String("path")).toString();

    if (hosts.isEmpty() || path.isEmpty()) {
        emit failed(tr("pCloud API response did not include a download URL."));
        return;
    }

    startCdnDownload(QStringLiteral("https://") + hosts.first().toString() + path);
}

// WeTransfer download (multi-step):
//   1. we.tl short links redirect to wetransfer.com/downloads/{id}/{hash} —
//      resolve that first (onWeTransferResolved) to read the id and hash.
//   2. POST the transfer id + security hash to the WeTransfer API, which
//      returns a short-lived direct_link to the file on its CDN.
//   3. Download that direct_link via the normal streaming path.
void CloudDownloader::downloadWeTransfer(const QUrl &url)
{
    if (url.host().toLower() == QLatin1String("we.tl")) {
        // Resolve the short link WITHOUT following the redirect, so we can read
        // the transfer id + security hash from the Location header.
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));
        req.setTransferTimeout(std::chrono::seconds(30));
        m_wetransferReply = m_nam->get(req);
        connect(m_wetransferReply, &QNetworkReply::finished, this, &CloudDownloader::onWeTransferResolved);
        return;
    }

    // Already a wetransfer.com/downloads/... link — go straight to the API.
    requestWeTransferLink(url);
}

void CloudDownloader::onWeTransferResolved()
{
    if (m_aborted) {
        m_wetransferReply->deleteLater();
        m_wetransferReply = nullptr;
        return;
    }

    const QUrl redirect = m_wetransferReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    const QNetworkReply::NetworkError err = m_wetransferReply->error();
    const QString errStr = m_wetransferReply->errorString();
    const QUrl base = m_wetransferReply->url();
    m_wetransferReply->deleteLater();
    m_wetransferReply = nullptr;

    if (!redirect.isValid()) {
        if (err != QNetworkReply::NoError)
            emit failed(tr("Could not resolve the WeTransfer link: %1").arg(errStr));
        else
            emit failed(tr("This WeTransfer link did not resolve to a download page."));
        return;
    }

    requestWeTransferLink(base.resolved(redirect));
}

// Step 2 (WeTransfer): POST the transfer id + security hash to the API to
// obtain the CDN direct_link.
void CloudDownloader::requestWeTransferLink(const QUrl &downloadsUrl)
{
    // Path: /downloads/{transfer_id}/{security_hash}
    //   or  /downloads/{transfer_id}/{recipient_id}/{security_hash}  (email links)
    const QStringList parts = downloadsUrl.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts.first() != QLatin1String("downloads")) {
        emit failed(tr("Unrecognized WeTransfer link. Use the full share link "
                       "(we.tl/\xe2\x80\xa6 or wetransfer.com/downloads/\xe2\x80\xa6)."));
        return;
    }

    const QString transferId = parts.at(1);
    QString recipientId;
    QString securityHash;
    if (parts.size() >= 4) {
        recipientId = parts.at(2);
        securityHash = parts.at(3);
    } else {
        securityHash = parts.at(2);
    }

    QJsonObject body;
    body.insert(QStringLiteral("security_hash"), securityHash);
    body.insert(QStringLiteral("intent"), QStringLiteral("entire_transfer"));
    if (!recipientId.isEmpty())
        body.insert(QStringLiteral("recipient_id"), recipientId);

    QUrl apiUrl(QStringLiteral("https://wetransfer.com/api/v4/transfers/") + transferId + QStringLiteral("/download"));
    QNetworkRequest req(apiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));
    req.setRawHeader("Referer", downloadsUrl.toString(QUrl::RemoveQuery).toUtf8());
    req.setTransferTimeout(std::chrono::seconds(30));

    m_wetransferReply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_wetransferReply, &QNetworkReply::finished, this, &CloudDownloader::onWeTransferApiFinished);
}

// Step 3 (WeTransfer): parse direct_link and start the actual download.
void CloudDownloader::onWeTransferApiFinished()
{
    if (m_aborted) {
        m_wetransferReply->deleteLater();
        m_wetransferReply = nullptr;
        return;
    }

    const QByteArray response = m_wetransferReply->readAll();
    const QNetworkReply::NetworkError err = m_wetransferReply->error();
    const QString errStr = m_wetransferReply->errorString();
    m_wetransferReply->deleteLater();
    m_wetransferReply = nullptr;

    const QJsonObject obj = QJsonDocument::fromJson(response).object();
    const QString directLink = obj.value(QLatin1String("direct_link")).toString();

    if (directLink.isEmpty()) {
        // WeTransfer returns {"message": "..."} on expired/invalid transfers.
        const QString msg = obj.value(QLatin1String("message")).toString();
        if (!msg.isEmpty())
            emit failed(tr("WeTransfer: %1").arg(msg));
        else if (err != QNetworkReply::NoError)
            emit failed(tr("WeTransfer API request failed: %1").arg(errStr));
        else
            emit failed(tr("This WeTransfer link has expired or is no longer available."));
        return;
    }

    // Prefer the real filename from the CDN URL for the saved temp file.
    const QString name = QFileInfo(QUrl(directLink).path()).fileName();
    if (name.endsWith(QLatin1String(".wpress"), Qt::CaseInsensitive))
        m_suggestedName = name;

    startCdnDownload(directLink);
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

    req.setTransferTimeout(std::chrono::seconds(30));
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

    m_aesCtr = mega_aes_ctr_new(reinterpret_cast<const unsigned char *>(m_megaCtx.aesKey.constData()), ivBlock);
    if (!m_aesCtr) {
        emit failed(tr("Failed to initialize AES decryption context."));
        return;
    }

    startCdnDownload(cdnUrl);
}

// Common path: open temp file and issue the GET for the CDN URL.
void CloudDownloader::startCdnDownload(const QString &cdnUrl)
{
    const QUrl cdnQUrl(cdnUrl);

    // Enforce HTTPS on the *resolved* URL too: the pCloud/Mega/WeTransfer APIs
    // hand back a download URL, and a plaintext one would defeat the HTTPS-only
    // guarantee applied at the front door (isRemoteUrl).
    if (cdnQUrl.scheme().toLower() != QLatin1String("https")) {
        emit failed(tr("The provider returned a non-HTTPS download URL, which was refused "
                       "to keep the download safe from tampering."));
        return;
    }

    m_tempFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/traktor-XXXXXX.wpress"), this);
    m_tempFile->setAutoRemove(false); // MainWindow takes ownership on success
    if (!m_tempFile->open()) {
        const QString err = m_tempFile->errorString();
        cleanupTempFile();
        emit failed(tr("Could not create temporary file: %1").arg(err));
        return;
    }

    QNetworkRequest req(cdnQUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Traktor/1.0 (WPRESS extractor; https://traktor.wp-migration.com)"));
    req.setTransferTimeout(std::chrono::seconds(30)); // don't wedge the UI on a stalled connection

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &CloudDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &CloudDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &CloudDownloader::onFinished);
}

// QNetworkReply::abort() emits finished() synchronously, and our finished-slots
// null the member pointer in their m_aborted branch — so aborting in place would
// deleteLater() a pointer the slot just cleared (dereferencing a dangling member →
// SIGSEGV). Disconnect first so the slot cannot re-enter, and clear the member
// before abort()/deleteLater().
static void killReply(QNetworkReply *&reply)
{
    if (!reply)
        return;
    QNetworkReply *r = reply;
    reply = nullptr; // must precede abort(): abort() re-enters our slots
    r->disconnect();
    r->abort();
    r->deleteLater();
}

void CloudDownloader::abort()
{
    m_aborted = true;
    killReply(m_wetransferReply);
    killReply(m_pcloudApiReply);
    killReply(m_megaApiReply);
    killReply(m_reply);
    cleanupAes();
    cleanupTempFile(); // remove partial download from disk
}

// ── Private slots ─────────────────────────────────────────────────────────────

void CloudDownloader::onReadyRead()
{
    if (!m_tempFile || !m_reply)
        return;

    QByteArray chunk = m_reply->readAll();
    if (chunk.isEmpty())
        return;

    writeChunk(std::move(chunk));
}

// AES-128-CTR is a stream cipher: output length equals input length, decryption
// never fails, and the keystream carries across calls, so we decrypt each chunk in
// place. For non-Mega downloads the bytes are written through unchanged.
void CloudDownloader::writeChunk(QByteArray data)
{
    if (!m_tempFile || m_writeFailed)
        return;

    if (m_isMega && m_aesCtr)
        mega_aes_ctr_xcrypt(m_aesCtr, reinterpret_cast<unsigned char *>(data.data()), static_cast<size_t>(data.size()));

    // A short write (disk full, read-only volume) must never be treated as success —
    // record it and let onFinished() fail instead of opening a truncated .wpress.
    if (m_tempFile->write(data) != data.size())
        m_writeFailed = true;
}

void CloudDownloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
        emit progress(static_cast<int>(received * 100 / total));
    else
        emit progress(-1);
}

// Extracts a filename from a Content-Disposition header value, or "" if absent.
// Handles filename="x", bare filename=x, and RFC 5987 filename*=charset''pct-encoded.
static QString filenameFromContentDisposition(const QByteArray &header)
{
    const QString value = QString::fromUtf8(header);
    if (value.isEmpty())
        return QString();

    // RFC 5987 extended form takes precedence: filename*=UTF-8''percent-encoded
    static const QRegularExpression extRe(QStringLiteral("filename\\*\\s*=\\s*[^']*'[^']*'([^;]+)"),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = extRe.match(value);
    if (m.hasMatch())
        return QUrl::fromPercentEncoding(m.captured(1).trimmed().toUtf8());

    static const QRegularExpression re(QStringLiteral("filename\\s*=\\s*\"([^\"]*)\"|filename\\s*=\\s*([^;]+)"),
                                       QRegularExpression::CaseInsensitiveOption);
    m = re.match(value);
    if (m.hasMatch())
        return (m.captured(1).isEmpty() ? m.captured(2) : m.captured(1)).trimmed();

    return QString();
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

    // Flush any remaining buffered data (before checking for errors). writeChunk()
    // decrypts it in place for Mega. CTR has no final block, so there is nothing to
    // finalize; just release the context once the stream is drained.
    if (m_tempFile && m_reply)
        writeChunk(m_reply->readAll());

    if (m_isMega && m_aesCtr)
        cleanupAes();

    const QNetworkReply::NetworkError err = m_reply->error();
    const QString errStr = m_reply->errorString();
    const int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString cdName = filenameFromContentDisposition(m_reply->rawHeader("Content-Disposition"));

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

    if (m_writeFailed) {
        cleanupTempFile();
        emit failed(tr("Could not write the download to disk. The temporary volume "
                       "may be full or read-only."));
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

    // Prefer the server-provided filename when it names a .wpress — e.g. Google
    // Drive serves the real name while the share URL path is just ".../view".
    // Take the basename only, so a header can't smuggle path components into the
    // display label.
    const QString cdBase = QFileInfo(cdName).fileName();
    if (cdBase.endsWith(QLatin1String(".wpress"), Qt::CaseInsensitive))
        m_suggestedName = cdBase;

    emit finished(m_tempFile->fileName(), m_suggestedName);
}
