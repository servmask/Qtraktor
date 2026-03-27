#include <QtTest>
#include <QSettings>

class TestQSettings : public QObject
{
    Q_OBJECT

private slots:
    void testLastOpenPathPersistence()
    {
        const QString testPath = "/tmp/test/open/path";

        {
            QSettings settings("com.servmask", "Traktor-Test");
            settings.setValue("lastOpenPath", testPath);
        }

        {
            QSettings settings("com.servmask", "Traktor-Test");
            QCOMPARE(settings.value("lastOpenPath").toString(), testPath);
        }
    }

    void testLastExtractPathPersistence()
    {
        const QString testPath = "/tmp/test/extract/path";

        {
            QSettings settings("com.servmask", "Traktor-Test");
            settings.setValue("lastExtractPath", testPath);
        }

        {
            QSettings settings("com.servmask", "Traktor-Test");
            QCOMPARE(settings.value("lastExtractPath").toString(), testPath);
        }
    }

    void testWindowGeometryPersistence()
    {
        const QByteArray testGeometry("fake-geometry-data-for-test");

        {
            QSettings settings("com.servmask", "Traktor-Test");
            settings.setValue("windowGeometry", testGeometry);
        }

        {
            QSettings settings("com.servmask", "Traktor-Test");
            QCOMPARE(settings.value("windowGeometry").toByteArray(), testGeometry);
        }
    }

    void testDefaultValues()
    {
        QSettings settings("com.servmask", "Traktor-Test-Fresh");
        QVERIFY(settings.value("lastOpenPath").toString().isEmpty());
        QVERIFY(settings.value("lastExtractPath").toString().isEmpty());
        QVERIFY(settings.value("windowGeometry").toByteArray().isEmpty());

        // Clean up
        settings.clear();
    }

    void cleanupTestCase()
    {
        QSettings settings("com.servmask", "Traktor-Test");
        settings.clear();
    }
};

int runTestQSettings(int argc, char **argv)
{
    TestQSettings test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qsettings.moc"
