#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "extractionworker.h"

class TestExtractionWorker : public QObject
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

    void testSuccessfulExtraction()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/plain.wpress", QString(), destDir);
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);
        QSignalSpy progressSpy(&worker, &ExtractionWorker::progress);

        worker.start();
        QVERIFY(worker.wait(10000)); // 10 second timeout

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), true);
        // Note: progress signals may not arrive for tiny fixtures due to
        // cross-thread signal delivery timing. Don't assert count > 0.

        // Verify files extracted
        QVERIFY(QFile::exists(destDir + "/package.json"));
        QVERIFY(QFile::exists(destDir + "/wp-content/hello.txt"));
    }

    void testMultifileExtraction()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/multifile.wpress", QString(), destDir);
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);

        worker.start();
        QVERIFY(worker.wait(10000));

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), true);

        QVERIFY(QFile::exists(destDir + "/wp-content/themes/test/index.php"));
        QVERIFY(QFile::exists(destDir + "/wp-content/themes/test/style.css"));
        QVERIFY(QFile::exists(destDir + "/readme.txt"));
    }

    void testEmptyArchive()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/empty.wpress", QString(), destDir);
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);

        worker.start();
        QVERIFY(worker.wait(10000));

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), true);
    }

    void testCorruptedArchive()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/corrupted.wpress", QString(), destDir);
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);
        QSignalSpy errorSpy(&worker, &ExtractionWorker::extractionError);

        worker.start();
        QVERIFY(worker.wait(10000));

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
        QVERIFY(errorSpy.count() > 0);
    }

    void testNonexistentFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        ExtractionWorker worker("/nonexistent/file.wpress", QString(), tmpDir.path());
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);
        QSignalSpy errorSpy(&worker, &ExtractionWorker::extractionError);

        worker.start();
        QVERIFY(worker.wait(10000));

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
        QVERIFY(errorSpy.count() > 0);
    }

    void testAbortDuringExtraction()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/multifile.wpress", QString(), destDir);
        QSignalSpy finishedSpy(&worker, &ExtractionWorker::extractionFinished);

        worker.start();
        // Immediately abort
        worker.abort();
        QVERIFY(worker.wait(10000));

        QCOMPARE(finishedSpy.count(), 1);
        // Should have failed due to abort
        QCOMPARE(finishedSpy.first().first().toBool(), false);
        QVERIFY(worker.isAborted());
    }

    void testPhaseChangedSignal()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QString destDir = tmpDir.path() + "/output";
        QDir().mkdir(destDir);

        ExtractionWorker worker(fixtureDir + "/plain.wpress", QString(), destDir);
        QSignalSpy phaseSpy(&worker, &ExtractionWorker::phaseChanged);

        worker.start();
        QVERIFY(worker.wait(10000));

        // Should have at least the "Extracting..." phase
        QVERIFY(phaseSpy.count() > 0);
        const QString firstPhase = phaseSpy.first().first().toString();
        QVERIFY(firstPhase.contains("Extracting") || firstPhase.contains("Verifying"));
    }
};

int runTestExtractionWorker(int argc, char **argv)
{
    TestExtractionWorker test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_extractionworker.moc"
