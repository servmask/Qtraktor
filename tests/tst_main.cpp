#include <QApplication>
#include <QtTest>

// Forward declarations of test classes
class TestBackupFile;
class TestCryptoUtilsStreaming;
class TestExtractionWorker;
class TestQSettings;

// Test entry point: runs all test classes
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("com.servmask");
    app.setApplicationName("Traktor-Test");

    int status = 0;

    {
        extern int runTestBackupFile(int, char**);
        status |= runTestBackupFile(argc, argv);
    }
    {
        extern int runTestCryptoUtilsStreaming(int, char**);
        status |= runTestCryptoUtilsStreaming(argc, argv);
    }
    {
        extern int runTestExtractionWorker(int, char**);
        status |= runTestExtractionWorker(argc, argv);
    }
    {
        extern int runTestQSettings(int, char**);
        status |= runTestQSettings(argc, argv);
    }

    return status;
}
