#include "setupdialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Set Up Traktor"));
    setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_agents = m_mgr.detectAgents();

    bool anyDetected = false;
    for (const AgentInfo &agent : m_agents) {
        if (agent.detected)
            anyDetected = true;
    }

    if (anyDetected) {
        layout->addWidget(new QLabel(tr("Traktor can integrate with your AI coding tools.\n"
                                        "Select which agents to register as MCP tool providers:")));
        layout->addSpacing(10);

        for (int i = 0; i < m_agents.size(); i++) {
            const AgentInfo &agent = m_agents.at(i);
            if (!agent.detected)
                continue;

            QCheckBox *cb = new QCheckBox(agent.name);
            cb->setChecked(true);
            if (agent.registered) {
                cb->setText(agent.name + " (already registered)");
                cb->setChecked(true);
            }
            layout->addWidget(cb);
            m_checkboxes.append(cb);
        }
    } else {
        layout->addWidget(new QLabel(tr("No AI coding agents detected on this system.\n\n"
                                        "Install Claude Code, Gemini CLI, or other supported agents,\n"
                                        "then run this setup again from Tools > Manage AI Agent Integrations.")));
    }

    layout->addSpacing(10);

    QDialogButtonBox *buttons = new QDialogButtonBox();
    if (anyDetected) {
        QPushButton *setupBtn = buttons->addButton(tr("Set Up"), QDialogButtonBox::AcceptRole);
        connect(setupBtn, &QPushButton::clicked, this, &SetupDialog::onSetup);
    }
    QPushButton *skipBtn = buttons->addButton(tr("Skip"), QDialogButtonBox::RejectRole);
    connect(skipBtn, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SetupDialog::onSetup()
{
    int registered = 0;
    int cbIndex = 0;

    for (int i = 0; i < m_agents.size(); i++) {
        const AgentInfo &agent = m_agents.at(i);
        if (!agent.detected)
            continue;

        if (cbIndex < m_checkboxes.size() && m_checkboxes.at(cbIndex)->isChecked()) {
            QString errorMsg;
            if (m_mgr.registerAgent(agent.type, &errorMsg)) {
                registered++;
            }
        }
        cbIndex++;
    }

    if (registered > 0) {
        QMessageBox::information(this, tr("Setup Complete"),
                                 tr("Registered Traktor with %1 AI agent(s).\n"
                                    "They will discover Traktor automatically.")
                                     .arg(registered));
    }

    accept();
}
