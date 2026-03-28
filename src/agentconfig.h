#ifndef AGENTCONFIG_H
#define AGENTCONFIG_H

#include <QString>
#include <QList>
#include <QStringList>

enum AgentType { AGENT_CLAUDE, AGENT_GEMINI };

struct AgentInfo {
    AgentType type;
    QString name;
    QString configPath;
    bool detected;
    bool registered;
};

class AgentConfigManager
{
public:
    // configRoot override for testing (default: QDir::homePath())
    explicit AgentConfigManager(const QString &configRoot = QString());

    QList<AgentInfo> detectAgents();
    bool registerAgent(AgentType type, QString *errorMsg = nullptr);
    bool unregisterAgent(AgentType type, QString *errorMsg = nullptr);
    int registerAllDetected(QStringList *messages = nullptr);
    int unregisterAll(QStringList *messages = nullptr);

private:
    QString m_configRoot;
    QString configPathFor(AgentType type) const;
    QString agentName(AgentType type) const;
    bool isAgentDetected(AgentType type) const;
    bool isAgentRegistered(AgentType type) const;
    QString resolveTraktorCommand() const;
};

#endif // AGENTCONFIG_H
