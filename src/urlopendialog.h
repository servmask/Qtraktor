#ifndef URLOPENDIALOG_H
#define URLOPENDIALOG_H

#include <QDialog>
#include <QUrl>

class QLineEdit;
class QPushButton;

// Dialog that lets the user paste a direct download URL or a cloud share link
// for a .wpress backup. Accepted URL is retrieved via url().
class UrlOpenDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UrlOpenDialog(QWidget *parent = nullptr);

    QUrl url() const;

private slots:
    void onTextChanged(const QString &text);

private:
    QLineEdit *m_urlEdit;
    QPushButton *m_okButton;
};

#endif // URLOPENDIALOG_H
