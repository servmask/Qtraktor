#ifndef BACKUPFILE_H
#define BACKUPFILE_H
#include <QFile>
#include <QDir>

class BackupFile : public QFile
{
  Q_OBJECT
  public:
    BackupFile(const QString& filename)
      : QFile(filename),
        bytesRead(0),
        eof(4377, '\0')
    {}

    bool isValid()
    {
      if (!seek(size() - 4377)) {
        return false;
      }

      if (read(4377) != eof) {
        return false;
      }

      if (!seek(0)) {
        return false;
      }

      return true;
    }

    bool extract(QDir extractTo)
    {
      while (!atEnd()) {
        QByteArray header = read(4377);
        if (header == eof) {
          return true;
        }

        QString fileName = header.chopped(255).constData();
        header.remove(0, 255);

        bool ok;
        qint64 fileSize = header.chopped(14).toInt(&ok);
        if (!ok) {
          return false;
        }

        header.remove(0, 14);
        header.remove(0, 12);

        QDir filePath(extractTo.path() + "/" + header.constData());
        if (!QDir().exists(filePath.path())) {
          if (!QDir().mkpath(filePath.path())) {
            return false;
          }
        }

        QFile file(filePath.path() + "/" + fileName);
        if (!file.open(QIODevice::WriteOnly)) {
          return false;
        }

        while (fileSize > 0) {
          qint64 chunk = fileSize > 5 * 1024 * 1024 ? 5 * 1024 * 1024 : fileSize;
          file.write(read(chunk));
          fileSize -= chunk;
        }

        file.close();
      }

      return false;
    }

  signals:
    void progress(float percent);

  protected:
    qint64 readData(char* data, qint64 maxlen)
    {
      qint64 _bytesRead = QFile::readData(data, maxlen);
      bytesRead += _bytesRead;

      emit progress((bytesRead / size()) * 100);
      return _bytesRead;
    }

  private:
    qint64 bytesRead;
    QByteArray eof;
};

#endif // BACKUPFILE_H
