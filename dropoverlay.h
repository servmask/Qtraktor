#ifndef DROPOVERLAY_H
#define DROPOVERLAY_H

#include <QWidget>

class DropOverlay : public QWidget
{
    Q_OBJECT

  public:
    explicit DropOverlay(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // DROPOVERLAY_H
