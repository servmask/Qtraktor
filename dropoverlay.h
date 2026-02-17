#ifndef DROPOVERLAY_H
#define DROPOVERLAY_H

#include <QWidget>

class DropOverlay : public QWidget
{
    Q_OBJECT

  public:
    explicit DropOverlay(QWidget *parent = nullptr);
    void setHighlighted(bool highlighted);
    void setFileName(const QString &name);

  signals:
    void fileDropped(const QString &filePath);
    void clicked();

  protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

  private:
    bool m_highlighted = false;
    QString m_fileName;
};

#endif // DROPOVERLAY_H
