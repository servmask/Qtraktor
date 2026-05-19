#include "urlopendialog.h"
#include "clouddownloader.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

UrlOpenDialog::UrlOpenDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Open Backup from URL"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 16);

    auto *titleLabel = new QLabel(tr("<b>Open backup from cloud storage or URL</b>"), this);
    layout->addWidget(titleLabel);

    auto *descLabel = new QLabel(tr("Paste a direct download link or share URL from any of the supported providers "
                                    "below. Traktor will download the file in the background \xe2\x80\x94 no manual "
                                    "download needed.\n\n"
                                    "Supported: Google Drive \xe2\x80\xa2 Dropbox \xe2\x80\xa2 OneDrive \xe2\x80\xa2 "
                                    "Box \xe2\x80\xa2 pCloud \xe2\x80\xa2 Mega \xe2\x80\xa2 Amazon S3 \xe2\x80\xa2 "
                                    "Google Cloud Storage \xe2\x80\xa2 Azure Blob Storage \xe2\x80\xa2 DigitalOcean "
                                    "Spaces \xe2\x80\xa2 Any direct HTTPS link"),
                                 this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(descLabel);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("https://"));
    m_urlEdit->setClearButtonEnabled(true);
    layout->addWidget(m_urlEdit);

    auto *hintLabel = new QLabel(tr("<small>Tip: for Google Drive and OneDrive, use &quot;Share &rarr; Anyone with "
                                    "the link&quot; before pasting. For Mega, copy the link from &quot;Share &rarr; "
                                    "Copy link&quot; &mdash; the decryption key after the # must be included. "
                                    "For Amazon S3, use a pre-signed URL or make the object public.</small>"),
                                 this);
    hintLabel->setWordWrap(true);
    hintLabel->setTextFormat(Qt::RichText);
    layout->addWidget(hintLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_okButton = buttons->addButton(tr("Download && Open"), QDialogButtonBox::AcceptRole);
    m_okButton->setEnabled(false);
    layout->addWidget(buttons);

    connect(m_urlEdit, &QLineEdit::textChanged, this, &UrlOpenDialog::onTextChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_urlEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_okButton->isEnabled())
            accept();
    });
}

QUrl UrlOpenDialog::url() const
{
    return QUrl::fromUserInput(m_urlEdit->text().trimmed());
}

void UrlOpenDialog::onTextChanged(const QString &text)
{
    const QUrl candidate = QUrl::fromUserInput(text.trimmed());
    m_okButton->setEnabled(CloudDownloader::isRemoteUrl(candidate));
}
