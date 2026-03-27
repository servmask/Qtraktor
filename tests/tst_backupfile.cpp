#include <QtTest>
#include <QTemporaryDir>
#include "backupfile.h"
#include "extractionworker.h"

class TestBackupFile : public QObject
{
    Q_OBJECT

private:
    QString fixtureDir;

private slots:
    void initTestCase()
    {
        fixtureDir = QCoreApplication::applicationDirPath() + "/fixtures";
        if (!QDir(fixtureDir).exists()) {
            fixtureDir = QCoreApplication::applicationDirPath() + "/../fixtures";
        }
        QVERIFY2(QDir(fixtureDir).exists(), "Fixture directory not found");
    }

    void testCheckConfigPlain()
    {
        CheckResult result = ExtractionWorker::checkConfig(fixtureDir + "/plain.wpress");
        QVERIFY(result.isValid);
        QVERIFY(!result.isEncrypted);
        QCOMPARE(result.compressionType, COMPRESSION_NONE);
        QVERIFY(!result.isV2);
    }

    void testCheckConfigEmpty()
    {
        CheckResult result = ExtractionWorker::checkConfig(fixtureDir + "/empty.wpress");
        QVERIFY(result.isValid);
        QVERIFY(!result.isEncrypted);
    }

    void testCheckConfigCorrupted()
    {
        CheckResult result = ExtractionWorker::checkConfig(fixtureDir + "/corrupted.wpress");
        QVERIFY(!result.isValid);
    }

    void testCheckConfigNonexistent()
    {
        CheckResult result = ExtractionWorker::checkConfig("/nonexistent/file.wpress");
        QVERIFY(!result.isValid);
    }

    void testIsValidPlain()
    {
        BackupFile bf(fixtureDir + "/plain.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());
        bf.close();
    }

    void testIsValidEmpty()
    {
        BackupFile bf(fixtureDir + "/empty.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());
        bf.close();
    }

    void testIsValidCorrupted()
    {
        BackupFile bf(fixtureDir + "/corrupted.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(!bf.isValid());
        bf.close();
    }

    void testExtractPlain()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        BackupFile bf(fixtureDir + "/plain.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());
        bf.ensureConfigLoaded();
        bf.setConfig(bf.isEncryptedFile(), bf.getCompressionType());

        QDir dest(tmpDir.path());
        QVERIFY(bf.extract(dest));
        bf.close();

        // Verify extracted files exist
        QVERIFY(QFile::exists(tmpDir.path() + "/package.json"));
        QVERIFY(QFile::exists(tmpDir.path() + "/wp-content/hello.txt"));

        // Verify content
        QFile hello(tmpDir.path() + "/wp-content/hello.txt");
        QVERIFY(hello.open(QIODevice::ReadOnly));
        QCOMPARE(hello.readAll(), QByteArray("Hello, World!\n"));
    }

    void testExtractMultifile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        BackupFile bf(fixtureDir + "/multifile.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());
        bf.ensureConfigLoaded();
        bf.setConfig(bf.isEncryptedFile(), bf.getCompressionType());

        QDir dest(tmpDir.path());
        QVERIFY(bf.extract(dest));
        bf.close();

        QVERIFY(QFile::exists(tmpDir.path() + "/package.json"));
        QVERIFY(QFile::exists(tmpDir.path() + "/wp-content/themes/test/index.php"));
        QVERIFY(QFile::exists(tmpDir.path() + "/wp-content/themes/test/style.css"));
        QVERIFY(QFile::exists(tmpDir.path() + "/readme.txt"));
    }

    void testExtractEmpty()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        BackupFile bf(fixtureDir + "/empty.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());

        QDir dest(tmpDir.path());
        QVERIFY(bf.extract(dest));
        bf.close();
    }

    void testAbortFlag()
    {
        QAtomicInt abortFlag(0);

        BackupFile bf(fixtureDir + "/multifile.wpress");
        QVERIFY(bf.open(QIODevice::ReadOnly));
        QVERIFY(bf.isValid());
        bf.ensureConfigLoaded();
        bf.setConfig(bf.isEncryptedFile(), bf.getCompressionType());

        // Set abort AFTER validation, before extraction
        bf.setAbortFlag(&abortFlag);
        abortFlag.storeRelease(1);

        QTemporaryDir tmpDir;
        QDir dest(tmpDir.path());
        // Extraction should fail due to abort
        QVERIFY(!bf.extract(dest));
        bf.close();
    }
};

int runTestBackupFile(int argc, char **argv)
{
    TestBackupFile test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_backupfile.moc"
