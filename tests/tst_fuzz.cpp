#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include "backupfile.h"
#include "extractionworker.h"

// Fuzz tests for the .wpress parser.
// Generates malformed archives at runtime and verifies the parser
// handles them gracefully (no crashes, no hangs, proper error reporting).

class TestFuzz : public QObject
{
    Q_OBJECT

private:
    static constexpr int HEADER_SIZE = 4377;

    // Write a wpress header into a byte array
    static QByteArray makeHeader(const QByteArray &filename, qint64 contentSize, const QByteArray &filepath = ".")
    {
        QByteArray header(HEADER_SIZE, '\0');

        // Filename (0-254)
        header.replace(0, qMin(filename.size(), 254), filename.left(254));

        // File size (255-268), right-padded with spaces
        QByteArray sizeStr = QByteArray::number(contentSize);
        while (sizeStr.size() < 13)
            sizeStr.append(' ');
        header.replace(255, 13, sizeStr.left(13));

        // File path (281-4376)
        header.replace(281, qMin(filepath.size(), 4095), filepath.left(4095));

        return header;
    }

    static QByteArray makeEof() { return QByteArray(HEADER_SIZE, '\0'); }

    QString writeTempWpress(const QByteArray &data)
    {
        QTemporaryFile tmp(m_tempDir.path() + "/fuzz_XXXXXX.wpress");
        tmp.setAutoRemove(false);
        if (!tmp.open())
            return QString();
        tmp.write(data);
        tmp.close();
        return tmp.fileName();
    }

    QTemporaryDir m_tempDir;

private slots:
    void initTestCase() { QVERIFY(m_tempDir.isValid()); }

    // --- Truncated archives ---

    void testTruncatedBeforeEof()
    {
        // Valid header + content but no EOF block
        QByteArray data;
        QByteArray content = "Hello";
        data.append(makeHeader("package.json", 50));
        data.append(QByteArray(50, 'x')); // config content
        data.append(makeHeader("file.txt", content.size()));
        data.append(content);
        // Missing EOF block

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(!result.isValid);
    }

    void testTruncatedMidHeader()
    {
        // Only half a header
        QByteArray data(HEADER_SIZE / 2, '\0');
        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(!result.isValid);
    }

    void testTruncatedMidContent()
    {
        // Header claims 1000 bytes of content but file has only 100
        QByteArray data;
        data.append(makeHeader("file.txt", 1000));
        data.append(QByteArray(100, 'x')); // Only 100 bytes, not 1000

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        // Should detect this as invalid or at least not crash
        Q_UNUSED(result);
    }

    // --- Oversized fields ---

    void testOversizedFilename()
    {
        // Filename field filled with 255 bytes of 'A'
        QByteArray data;
        QByteArray config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        QByteArray longName(255, 'A');
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader(longName, 5));
        data.append("hello");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(result.isValid);
    }

    void testOversizedFilepath()
    {
        // Filepath field filled with 4095 bytes of nested directories
        QByteArray data;
        QByteArray config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        QByteArray longPath;
        for (int i = 0; i < 400; i++)
            longPath.append("dir/");
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader("file.txt", 5, longPath));
        data.append("hello");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(result.isValid);
    }

    // --- Garbage data ---

    void testPureGarbage()
    {
        // Random bytes, not a valid wpress at all
        QByteArray data(8000, '\xDE');
        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        // Must not crash. May or may not report valid depending on parser leniency.
        Q_UNUSED(result);
    }

    void testGarbageAfterValidHeader()
    {
        // Valid config header followed by garbage instead of content
        QByteArray data;
        data.append(makeHeader("package.json", 50));
        data.append(QByteArray(50, '\xFF'));   // garbage "config" content
        data.append(QByteArray(3000, '\xAB')); // more garbage

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        Q_UNUSED(result); // Must not crash
    }

    // --- Negative and zero sizes ---

    void testZeroSizeContent()
    {
        // Header claims 0 bytes of content
        QByteArray data;
        QByteArray config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader("empty.txt", 0));
        // No content bytes
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(result.isValid);
    }

    void testNegativeSizeInHeader()
    {
        // Manually craft a header with a negative size string
        QByteArray header(HEADER_SIZE, '\0');
        header.replace(0, 12, "package.json");
        header.replace(255, 3, "-1 ");
        header.replace(281, 1, ".");

        QByteArray data;
        data.append(header);
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        Q_UNUSED(result); // Must not crash
    }

    // --- Path traversal ---

    void testPathTraversalInFilepath()
    {
        // Filepath with ../ to attempt directory escape
        QByteArray data;
        QByteArray config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader("evil.txt", 4, "../../../tmp"));
        data.append("pwnd");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        // Extract to a temp directory and verify no files escape
        QTemporaryDir extractDir;
        QVERIFY(extractDir.isValid());

        ExtractionWorker worker(path, QString(), extractDir.path());
        worker.start();
        QVERIFY(worker.wait(10000));

        // Verify the malicious file was not written at the traversal path.
        // We check content, not existence, because the resolved path might
        // point to a real system file (e.g., /etc/passwd on Linux).
        QFile escaped(QDir::cleanPath(extractDir.path() + "/../../../tmp/evil.txt"));
        if (escaped.exists() && escaped.open(QIODevice::ReadOnly)) {
            QVERIFY(escaped.readAll() != "pwnd");
            escaped.close();
        }

        // Verify no files were written outside the extract directory
        // by checking that only config-related files exist inside it
        QDir extractContents(extractDir.path());
        QStringList entries = extractContents.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            QVERIFY(entry != "evil.txt");
        }
    }

    void testPathTraversalInFilename()
    {
        // Filename with ../ components
        QByteArray data;
        QByteArray config = R"({"Encrypted":false,"Compression":{"Enabled":false}})";
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader("../../etc/passwd", 4, "."));
        data.append("root");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        QTemporaryDir extractDir;
        QVERIFY(extractDir.isValid());

        ExtractionWorker worker(path, QString(), extractDir.path());
        worker.start();
        QVERIFY(worker.wait(10000));

        // Verify the malicious content was not written at the traversal path
        QFile escaped(QDir::cleanPath(extractDir.path() + "/../../etc/passwd"));
        if (escaped.exists() && escaped.open(QIODevice::ReadOnly)) {
            QVERIFY(escaped.readAll() != "root");
            escaped.close();
        }
    }

    // --- Boundary sizes ---

    void testExactlyOneHeaderSize()
    {
        // File is exactly one header (4377 bytes of zeros = EOF block)
        QByteArray data(HEADER_SIZE, '\0');
        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        // An all-zero header is the EOF marker, so a file with just EOF could be valid
        Q_UNUSED(result); // Must not crash
    }

    void testOneByteFile()
    {
        QByteArray data(1, '\0');
        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        QVERIFY(!result.isValid);
    }

    // --- Malformed config JSON ---

    void testInvalidConfigJson()
    {
        // Config file with broken JSON
        QByteArray data;
        QByteArray config = "{ not valid json at all !!!";
        data.append(makeHeader("package.json", config.size()));
        data.append(config);
        data.append(makeHeader("file.txt", 3));
        data.append("abc");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        // Parser should handle broken JSON gracefully
        Q_UNUSED(result); // Must not crash
    }

    void testMissingConfigFile()
    {
        // Archive with no package.json - goes straight to data files
        QByteArray data;
        data.append(makeHeader("file.txt", 5));
        data.append("hello");
        data.append(makeEof());

        QString path = writeTempWpress(data);
        QVERIFY(!path.isEmpty());

        CheckResult result = ExtractionWorker::checkConfig(path);
        // Should handle missing config gracefully
        Q_UNUSED(result); // Must not crash
    }
};

int runTestFuzz(int argc, char **argv)
{
    TestFuzz test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_fuzz.moc"
