#include "passworddialog.h"
#include <QMessageBox>

PasswordDialog::PasswordDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Enter Password"));
    setModal(true);
    setMinimumWidth(300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *label = new QLabel(tr("This backup file is password protected.\nPlease enter the password:"), this);
    mainLayout->addWidget(label);

    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(tr("Password"));
    mainLayout->addWidget(passwordEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    okButton = new QPushButton(tr("OK"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);

    okButton->setDefault(true);
    okButton->setAutoDefault(true);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, &PasswordDialog::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &PasswordDialog::onCancelClicked);
    connect(passwordEdit, &QLineEdit::returnPressed, this, &PasswordDialog::onOkClicked);
}

QString PasswordDialog::getPassword() const
{
    return password;
}

void PasswordDialog::onOkClicked()
{
    password = passwordEdit->text();
    accept();
}

void PasswordDialog::onCancelClicked()
{
    reject();
}
