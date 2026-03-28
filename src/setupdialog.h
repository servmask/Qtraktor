#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QList>
#include "agentconfig.h"

class SetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = nullptr);

private slots:
    void onSetup();

private:
    QList<QCheckBox *> m_checkboxes;
    QList<AgentInfo> m_agents;
    AgentConfigManager m_mgr;
};

#endif // SETUPDIALOG_H
