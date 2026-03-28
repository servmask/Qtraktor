#include <QtTest>
#include <QBuffer>
#include <QTemporaryFile>
#include "cryptoutils.h"

class TestCryptoUtilsStreaming : public QObject
{
    Q_OBJECT

private slots:
    void testStreamPlainCopy()
    {
        QByteArray input("Hello, World! This is a test of plain streaming.");
        QBuffer source(&input);
        source.open(QIODevice::ReadOnly);

        QByteArray output;
        QBuffer dest(&output);
        dest.open(QIODevice::WriteOnly);

        QString error;
        QVERIFY(CryptoUtils::processFileContentStreaming(&source, input.size(), &dest, false, "test.txt",
                                                         COMPRESSION_NONE, &error));
        QVERIFY(error.isEmpty());
        QCOMPARE(output, input);
    }

    void testStreamConfigFilePassthrough()
    {
        QByteArray input(R"({"Encrypted":false})");
        QBuffer source(&input);
        source.open(QIODevice::ReadOnly);

        QByteArray output;
        QBuffer dest(&output);
        dest.open(QIODevice::WriteOnly);

        QString error;
        // Even with compression flag true, config files should be passed through
        QVERIFY(CryptoUtils::processFileContentStreaming(&source, input.size(), &dest, true, "package.json",
                                                         COMPRESSION_ZLIB, &error));
        QVERIFY(error.isEmpty());
        QCOMPARE(output, input);
    }

    void testStreamEmptyContent()
    {
        QByteArray input;
        QBuffer source(&input);
        source.open(QIODevice::ReadOnly);

        QByteArray output;
        QBuffer dest(&output);
        dest.open(QIODevice::WriteOnly);

        QString error;
        QVERIFY(
            CryptoUtils::processFileContentStreaming(&source, 0, &dest, false, "empty.txt", COMPRESSION_NONE, &error));
        QVERIFY(error.isEmpty());
        QVERIFY(output.isEmpty());
    }

    void testStreamWithPasswordEmptyContentSize()
    {
        QByteArray input;
        QBuffer source(&input);
        source.open(QIODevice::ReadOnly);

        QByteArray output;
        QBuffer dest(&output);
        dest.open(QIODevice::WriteOnly);

        QString error;
        QVERIFY(CryptoUtils::processFileContentWithPasswordStreaming(&source, 0, &dest, false, "empty.txt", "password",
                                                                     COMPRESSION_NONE, &error));
        QVERIFY(error.isEmpty());
    }

    void testStreamLargePlainContent()
    {
        // Create 2MB of data to test multi-chunk streaming
        QByteArray input(2 * 1024 * 1024, 'A');
        QBuffer source(&input);
        source.open(QIODevice::ReadOnly);

        QByteArray output;
        QBuffer dest(&output);
        dest.open(QIODevice::WriteOnly);

        QString error;
        QVERIFY(CryptoUtils::processFileContentStreaming(&source, input.size(), &dest, false, "large.sql",
                                                         COMPRESSION_NONE, &error));
        QVERIFY(error.isEmpty());
        QCOMPARE(output.size(), input.size());
        QCOMPARE(output, input);
    }
};

int runTestCryptoUtilsStreaming(int argc, char **argv)
{
    TestCryptoUtilsStreaming test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_cryptoutils_streaming.moc"
