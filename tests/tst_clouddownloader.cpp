#include "clouddownloader.h"
#include "megaaes.h"
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

    void testIsRemoteUrl_wetransfer()
    {
        // we.tl / wetransfer.com are https, so isRemoteUrl() returns true.
        // download() routes them through the WeTransfer multi-step API flow.
        QVERIFY(CloudDownloader::isRemoteUrl(QUrl("https://we.tl/t-abcdEFGH")));
        QVERIFY(CloudDownloader::isRemoteUrl(QUrl("https://wetransfer.com/downloads/abc123/def456")));
    }

    void testIsRemoteUrl_empty() { QVERIFY(!CloudDownloader::isRemoteUrl(QUrl())); }

    // ── normalizeUrl: Google Drive ─────────────────────────────────────────

    void testNormalize_googleDrive_pathFormat()
    {
        QUrl input("https://drive.google.com/file/d/1abc123XYZ/view?usp=sharing");
        QUrl result = CloudDownloader::normalizeUrl(input);
        QCOMPARE(result.host(), QString("drive.usercontent.google.com"));
        QCOMPARE(result.path(), QString("/download"));
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

    // ── normalizeUrl: pCloud ───────────────────────────────────────────────

    void testNormalize_pcloud_unchanged()
    {
        // pCloud is handled as a two-step download inside download() (API call
        // to resolve CDN URL), not via normalizeUrl(). URLs pass through unchanged.
        QUrl input("https://u.pcloud.link/publink/show?code=xZaBcD");
        QCOMPARE(CloudDownloader::normalizeUrl(input), input);
    }

    // ── normalizeUrl: pass-through providers ──────────────────────────────

    void testNormalize_directUrl_unchanged()
    {
        // Direct / pre-signed HTTPS URLs (any host) must pass through untouched.
        QUrl input("https://files.example.com/backup.wpress"
                   "?token=abc&expires=3600");
        QCOMPARE(CloudDownloader::normalizeUrl(input), input);
    }

    void testNormalize_mega_unchanged()
    {
        // Mega must NOT be touched by normalizeUrl — download() handles it.
        QUrl input("https://mega.nz/file/ABCDEFGH#somekey");
        QCOMPARE(CloudDownloader::normalizeUrl(input), input);
    }

    void testNormalize_wetransfer_unchanged()
    {
        // WeTransfer is handled as a multi-step download inside download()
        // (resolve short link, POST to API for direct_link), not via
        // normalizeUrl(). URLs pass through unchanged.
        QUrl shortUrl("https://we.tl/t-abcdEFGH");
        QCOMPARE(CloudDownloader::normalizeUrl(shortUrl), shortUrl);
        QUrl dlUrl("https://wetransfer.com/downloads/abc123/def456");
        QCOMPARE(CloudDownloader::normalizeUrl(dlUrl), dlUrl);
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

    // ── AES-128-CTR (Mega stream decryption) ───────────────────────────────
    // NIST SP 800-38A F.5.1 CTR-AES128 test vector. CTR encrypt == decrypt, so a
    // single xcrypt of the plaintext must yield the published ciphertext. This
    // validates both the tiny-AES-c AES-128 core and megaaes.c's counter/keystream.
    void testMegaCtr_nistVector()
    {
        const QByteArray key = QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c");
        const QByteArray iv = QByteArray::fromHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        const QByteArray plain = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a"
                                                     "ae2d8a571e03ac9c9eb76fac45af8e51"
                                                     "30c81c46a35ce411e5fbc1191a0a52ef"
                                                     "f69f2445df4f9b17ad2b417be66c3710");
        const QByteArray cipher = QByteArray::fromHex("874d6191b620e3261bef6864990db6ce"
                                                      "9806f66b7970fdff8617187bb9fffdff"
                                                      "5ae4df3edbd5d35e5b4f09020db03eab"
                                                      "1e031dda2fbe03d1792170a0f3009cee");

        QByteArray buf = plain;
        mega_aes_ctr *ctx = mega_aes_ctr_new(reinterpret_cast<const uint8_t *>(key.constData()),
                                             reinterpret_cast<const uint8_t *>(iv.constData()));
        QVERIFY(ctx);
        mega_aes_ctr_xcrypt(ctx, reinterpret_cast<uint8_t *>(buf.data()), static_cast<size_t>(buf.size()));
        mega_aes_ctr_free(ctx);
        QCOMPARE(buf.toHex(), cipher.toHex());
    }

    // Feeding the stream in arbitrary, non-block-aligned chunks (as QNetworkReply
    // delivers them) must match the single-shot result. This is the property
    // tiny-AES-c's own AES_CTR_xcrypt_buffer lacks (it restarts the keystream block
    // on every call), and the reason CTR is reimplemented in megaaes.c.
    void testMegaCtr_chunkedMatchesSingleShot()
    {
        const QByteArray key = QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c");
        const QByteArray iv = QByteArray::fromHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        const QByteArray plain = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a"
                                                     "ae2d8a571e03ac9c9eb76fac45af8e51"
                                                     "30c81c46a35ce411e5fbc1191a0a52ef"
                                                     "f69f2445df4f9b17ad2b417be66c3710");
        const QByteArray cipher = QByteArray::fromHex("874d6191b620e3261bef6864990db6ce"
                                                      "9806f66b7970fdff8617187bb9fffdff"
                                                      "5ae4df3edbd5d35e5b4f09020db03eab"
                                                      "1e031dda2fbe03d1792170a0f3009cee");

        QByteArray buf = plain;
        mega_aes_ctr *ctx = mega_aes_ctr_new(reinterpret_cast<const uint8_t *>(key.constData()),
                                             reinterpret_cast<const uint8_t *>(iv.constData()));
        QVERIFY(ctx);
        // Deliberately non-block-aligned split points that end mid-block.
        const int splits[] = {1, 7, 20};
        int off = 0;
        for (int len : splits) {
            mega_aes_ctr_xcrypt(ctx, reinterpret_cast<uint8_t *>(buf.data()) + off, static_cast<size_t>(len));
            off += len;
        }
        mega_aes_ctr_xcrypt(ctx, reinterpret_cast<uint8_t *>(buf.data()) + off, static_cast<size_t>(buf.size() - off));
        mega_aes_ctr_free(ctx);
        QCOMPARE(buf.toHex(), cipher.toHex());
    }
};

#include "tst_clouddownloader.moc"

int runTestCloudDownloader(int argc, char **argv)
{
    TestCloudDownloader tc;
    return QTest::qExec(&tc, argc, argv);
}
