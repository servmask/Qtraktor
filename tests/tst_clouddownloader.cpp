#include "clouddownloader.h"
#include <QtTest>

class TestCloudDownloader : public QObject
{
    Q_OBJECT

private slots:
    // ── isRemoteUrl ───────────────────────────────────────────────────────

    void testIsRemoteUrl_http() { QVERIFY(CloudDownloader::isRemoteUrl(QUrl("http://example.com/backup.wpress"))); }

    void testIsRemoteUrl_https() { QVERIFY(CloudDownloader::isRemoteUrl(QUrl("https://example.com/backup.wpress"))); }

    void testIsRemoteUrl_localFile() { QVERIFY(!CloudDownloader::isRemoteUrl(QUrl("file:///tmp/backup.wpress"))); }

    void testIsRemoteUrl_ftp_notSupported()
    {
        // Qt 6 removed FTP from QNetworkAccessManager; ftp:// must be rejected.
        QVERIFY(!CloudDownloader::isRemoteUrl(QUrl("ftp://example.com/backup.wpress")));
    }

    void testIsRemoteUrl_mega()
    {
        // mega.nz uses https, so isRemoteUrl() returns true. The drop overlay
        // emits urlDropped, and download() then routes it through the Mega
        // two-step path (API call + AES-CTR decryption).
        QVERIFY(CloudDownloader::isRemoteUrl(QUrl("https://mega.nz/file/ABCD#KEY")));
    }

    void testIsRemoteUrl_empty() { QVERIFY(!CloudDownloader::isRemoteUrl(QUrl())); }

    // ── normalizeUrl: Google Drive ─────────────────────────────────────────

    void testNormalize_googleDrive_pathFormat()
    {
        QUrl input("https://drive.google.com/file/d/1abc123XYZ/view?usp=sharing");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result.host(), QString("drive.google.com"));
        QCOMPARE(result.path(), QString("/uc"));
        QUrlQuery q(result);
        QCOMPARE(q.queryItemValue("export"), QString("download"));
        QCOMPARE(q.queryItemValue("id"), QString("1abc123XYZ"));
        QCOMPARE(q.queryItemValue("confirm"), QString("t"));
    }

    void testNormalize_googleDrive_openFormat()
    {
        QUrl input("https://drive.google.com/open?id=1abc123XYZ");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QUrlQuery q(result);
        QCOMPARE(q.queryItemValue("id"), QString("1abc123XYZ"));
        QCOMPARE(q.queryItemValue("export"), QString("download"));
    }

    void testNormalize_googleDrive_noId_unchanged()
    {
        QUrl input("https://drive.google.com/drive/folders/xyz");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result, input); // folder links have no ID we can extract
    }

    // ── normalizeUrl: Dropbox ─────────────────────────────────────────────

    void testNormalize_dropbox_dl0()
    {
        QUrl input("https://www.dropbox.com/s/abc123/backup.wpress?dl=0");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(QUrlQuery(result).queryItemValue("dl"), QString("1"));
    }

    void testNormalize_dropbox_noDlParam()
    {
        QUrl input("https://www.dropbox.com/s/abc123/backup.wpress");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(QUrlQuery(result).queryItemValue("dl"), QString("1"));
    }

    void testNormalize_dropbox_noDomain()
    {
        QUrl input("https://dropbox.com/s/abc123/backup.wpress?dl=0");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(QUrlQuery(result).queryItemValue("dl"), QString("1"));
    }

    // ── normalizeUrl: OneDrive ─────────────────────────────────────────────

    void testNormalize_onedrive_shortLink()
    {
        QUrl input("https://1drv.ms/u/s!AaBbCcDd");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(QUrlQuery(result).queryItemValue("download"), QString("1"));
    }

    void testNormalize_onedrive_liveLink()
    {
        QUrl input("https://onedrive.live.com/redir?resid=ABC&authkey=XYZ");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(QUrlQuery(result).queryItemValue("download"), QString("1"));
    }

    // ── normalizeUrl: Box ─────────────────────────────────────────────────

    void testNormalize_box_sharedLink()
    {
        QUrl input("https://app.box.com/s/abc123xyz");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result.path(), QString("/shared/static/abc123xyz"));
        QCOMPARE(result.host(), QString("app.box.com"));
    }

    void testNormalize_box_otherPath_unchanged()
    {
        QUrl input("https://app.box.com/file/123456");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result, input);
    }

    // ── normalizeUrl: pCloud ───────────────────────────────────────────────

    void testNormalize_pcloud()
    {
        QUrl input("https://u.pcloud.link/publink/show?code=xZaBcD");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result.path(), QString("/publink/code"));
        QUrlQuery q(result);
        QCOMPARE(q.queryItemValue("code"), QString("xZaBcD"));
        QCOMPARE(q.queryItemValue("forcedownload"), QString("1"));
    }

    // ── normalizeUrl: pass-through providers ──────────────────────────────

    void testNormalize_s3_unchanged()
    {
        QUrl input("https://mybucket.s3.amazonaws.com/backup.wpress"
                   "?X-Amz-Signature=abc&X-Amz-Expires=3600");
        QCOMPARE(CloudDownloader::normalizeUrl(input), input);
    }

    void testNormalize_mega_unchanged()
    {
        // Mega must NOT be touched by normalizeUrl — download() handles it.
        QUrl input("https://mega.nz/file/ABCDEFGH#somekey");
        QCOMPARE(CloudDownloader::normalizeUrl(input), input);
    }

    // ── parseMegaUrl: new format ───────────────────────────────────────────

    void testParseMega_newFormat_valid()
    {
        // Construct a well-formed 32-byte key (base64url, no padding).
        // Key bytes (hex): 0102030405060708090a0b0c0d0e0f10  (first 16)
        //                  1112131415161718191a1b1c1d1e1f20  (second 16)
        // Expected AES key = XOR of both halves:
        //   aesKey[i] = key[i] ^ key[i+16]
        QByteArray rawKey(32, '\0');
        for (int i = 0; i < 32; ++i)
            rawKey[i] = static_cast<char>(i + 1);
        const QString keyStr =
            QString::fromLatin1(rawKey.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

        QUrl url(QString("https://mega.nz/file/ABCDEFGH#") + keyStr);
        CloudDownloader::MegaCtx ctx = CloudDownloader::parseMegaUrl(url);

        QVERIFY(ctx.valid);
        QCOMPARE(ctx.aesKey.size(), 16);
        QCOMPARE(ctx.iv.size(), 8);

        // Verify key derivation: aesKey = bytes[0..15] XOR bytes[16..31]
        for (int i = 0; i < 16; ++i) {
            const unsigned char expected =
                static_cast<unsigned char>(rawKey[i]) ^ static_cast<unsigned char>(rawKey[i + 16]);
            QCOMPARE(static_cast<unsigned char>(ctx.aesKey[i]), expected);
        }

        // Verify IV = bytes[16..23]
        QCOMPARE(ctx.iv, rawKey.mid(16, 8));
    }

    void testParseMega_legacyFormat_valid()
    {
        QByteArray rawKey(32, '\x42');
        const QString keyStr =
            QString::fromLatin1(rawKey.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

        QUrl url(QString("https://mega.nz/#!FILENODE!") + keyStr);
        CloudDownloader::MegaCtx ctx = CloudDownloader::parseMegaUrl(url);

        QVERIFY(ctx.valid);
        QCOMPARE(ctx.aesKey.size(), 16);
        QCOMPARE(ctx.iv.size(), 8);
        // All bytes are 0x42, so XOR of equal halves = 0x00
        for (int i = 0; i < 16; ++i)
            QCOMPARE(static_cast<unsigned char>(ctx.aesKey[i]), 0x00u);
    }

    void testParseMega_wrongHost()
    {
        QUrl url("https://notmega.com/file/ABCD#KEY");
        QVERIFY(!CloudDownloader::parseMegaUrl(url).valid);
    }

    void testParseMega_noKey()
    {
        QUrl url("https://mega.nz/file/ABCDEFGH");
        QVERIFY(!CloudDownloader::parseMegaUrl(url).valid);
    }

    void testParseMega_keyTooShort()
    {
        // 16 bytes is too short (need >= 32)
        QByteArray shortKey(16, '\xAA');
        const QString keyStr =
            QString::fromLatin1(shortKey.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
        QUrl url(QString("https://mega.nz/file/ABCDEFGH#") + keyStr);
        QVERIFY(!CloudDownloader::parseMegaUrl(url).valid);
    }

    void testParseMega_legacyFormat_noSeparator()
    {
        QUrl url("https://mega.nz/#!FILENODE_NO_KEY_SEP");
        QVERIFY(!CloudDownloader::parseMegaUrl(url).valid);
    }
};

#include "tst_clouddownloader.moc"

int runTestCloudDownloader(int argc, char **argv)
{
    TestCloudDownloader tc;
    return QTest::qExec(&tc, argc, argv);
}
