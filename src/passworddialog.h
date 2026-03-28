#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(QWidget *parent = nullptr);
    QString getPassword() const;

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    QLineEdit *passwordEdit;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QString password;
};

#endif // PASSWORDDIALOG_H
