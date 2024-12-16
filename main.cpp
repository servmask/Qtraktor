#include "mainwindow.h"
#include "appdelegate.h"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  MainWindow w;
  
  AppDelegate appDelegate(&w);
  a.installEventFilter(&appDelegate);
  
  if (argc > 1) {
    w.openBackupFile(QString::fromLocal8Bit(argv[1]));
  }
  
  w.show();

  return a.exec();
}
