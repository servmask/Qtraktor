#ifndef PROGRESSWINDOW_H
#define PROGRESSWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class ProgressWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressWindow(QWidget *parent = nullptr);

    void setFileName(const QString &name);
    void setPhase(const QString &phase);
    void setProgress(int percent);
    void setQueueInfo(int remaining);

signals:
    void cancelClicked();

private:
    QLabel *m_iconLabel;
    QLabel *m_fileLabel;
    QLabel *m_phaseLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelButton;
};

#endif // PROGRESSWINDOW_H
