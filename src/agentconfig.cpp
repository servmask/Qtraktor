#include "agentconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

AgentConfigManager::AgentConfigManager(const QString &configRoot)
    : m_configRoot(configRoot.isEmpty() ? QDir::homePath() : configRoot)
{
}

QString AgentConfigManager::agentName(AgentType type) const
{
    switch (type) {
    case AGENT_CLAUDE:
        return "Claude Code";
    case AGENT_GEMINI:
        return "Gemini CLI";
    }
    return "Unknown";
}

QString AgentConfigManager::configPathFor(AgentType type) const
{
    switch (type) {
    case AGENT_CLAUDE:
        return m_configRoot + "/.claude.json";
    case AGENT_GEMINI:
        return m_configRoot + "/.gemini/settings.json";
    }
    return QString();
}

// Search common binary locations (macOS GUI apps don't inherit shell PATH)
static bool findBinary(const QString &name)
{
    if (!QStandardPaths::findExecutable(name).isEmpty()) {
        return true;
    }
    QStringList searchDirs;
    searchDirs << "/usr/local/bin" << "/opt/homebrew/bin" << QDir::homePath() + "/.local/bin";
    for (const QString &dir : searchDirs) {
        if (QFile::exists(dir + "/" + name)) {
            return true;
        }
    }
    return false;
}

bool AgentConfigManager::isAgentDetected(AgentType type) const
{
    switch (type) {
    case AGENT_CLAUDE:
        return QFile::exists(configPathFor(AGENT_CLAUDE)) || findBinary("claude");
    case AGENT_GEMINI:
        return QDir(m_configRoot + "/.gemini").exists() || findBinary("gemini");
    }
    return false;
}

bool AgentConfigManager::isAgentRegistered(AgentType type) const
{
    const QString path = configPathFor(type);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject servers = root["mcpServers"].toObject();
    return servers.contains("traktor");
}

QString AgentConfigManager::resolveTraktorCommand() const
{
    // Always use absolute path to avoid PATH dependency
    return QCoreApplication::applicationFilePath();
}

QList<AgentInfo> AgentConfigManager::detectAgents()
{
    QList<AgentInfo> agents;
    const AgentType types[] = {AGENT_CLAUDE, AGENT_GEMINI};

    for (AgentType type : types) {
        AgentInfo info;
        info.type = type;
        info.name = agentName(type);
        info.configPath = configPathFor(type);
        info.detected = isAgentDetected(type);
        info.registered = info.detected ? isAgentRegistered(type) : false;
        agents.append(info);
    }

    return agents;
}

bool AgentConfigManager::registerAgent(AgentType type, QString *errorMsg)
{
    const QString path = configPathFor(type);
    if (path.isEmpty()) {
        if (errorMsg)
            *errorMsg = "Unknown agent type";
        return false;
    }

    // Ensure directory exists
    QFileInfo fi(path);
    if (!fi.dir().exists()) {
        QDir().mkpath(fi.dir().absolutePath());
    }

    QJsonObject root;

    // Read existing config
    QFile file(path);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
            file.close();

            if (parseErr.error != QJsonParseError::NoError) {
                // Invalid JSON — backup and start fresh
                const QString backupPath = path + ".bak";
                QFile::remove(backupPath);
                QFile::copy(path, backupPath);
                if (errorMsg) {
                    *errorMsg = QString("Backed up invalid %1 to %2").arg(path, backupPath);
                }
            } else if (doc.isObject()) {
                root = doc.object();
            }
        }
    }

    // Build traktor config entry
    const QString command = resolveTraktorCommand();
    QJsonObject traktorConfig;
    traktorConfig["command"] = command;
    QJsonArray argsArray;
    argsArray.append("mcp");
    traktorConfig["args"] = argsArray;

    // Get or create mcpServers
    QJsonObject mcpServers = root["mcpServers"].toObject();

    // Check if already registered with the same command path (idempotent)
    if (mcpServers.contains("traktor")) {
        QJsonObject existing = mcpServers["traktor"].toObject();
        if (existing["command"].toString() == command) {
            return true; // Already registered correctly
        }
        // Stale path — update it
    }

    mcpServers["traktor"] = traktorConfig;
    root["mcpServers"] = mcpServers;

    // Write atomically via QSaveFile (writes to temp, renames on commit)
    QSaveFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        if (errorMsg)
            *errorMsg = QString("Cannot write to %1").arg(path);
        return false;
    }
    saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        if (errorMsg)
            *errorMsg = QString("Failed to commit write to %1").arg(path);
        return false;
    }

    return true;
}

bool AgentConfigManager::unregisterAgent(AgentType type, QString *errorMsg)
{
    const QString path = configPathFor(type);
    QFile file(path);
    if (!file.exists()) {
        return true; // Nothing to unregister
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg)
            *errorMsg = QString("Cannot read %1").arg(path);
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return true; // Can't parse, nothing to remove
    }

    QJsonObject root = doc.object();
    QJsonObject mcpServers = root["mcpServers"].toObject();

    if (!mcpServers.contains("traktor")) {
        return true; // Not registered
    }

    mcpServers.remove("traktor");
    root["mcpServers"] = mcpServers;

    // Write atomically via QSaveFile (writes to temp, renames on commit)
    QSaveFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        if (errorMsg)
            *errorMsg = QString("Cannot write to %1").arg(path);
        return false;
    }
    saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        if (errorMsg)
            *errorMsg = QString("Failed to commit write to %1").arg(path);
        return false;
    }

    return true;
}

int AgentConfigManager::registerAllDetected(QStringList *messages)
{
    int count = 0;
    QList<AgentInfo> agents = detectAgents();

    for (const AgentInfo &agent : agents) {
        if (!agent.detected) {
            if (messages)
                messages->append(QString("Skipped %1 (not detected)").arg(agent.name));
            continue;
        }

        QString errorMsg;
        if (registerAgent(agent.type, &errorMsg)) {
            count++;
            if (messages)
                messages->append(QString("Registered with %1 (%2)").arg(agent.name, agent.configPath));
        } else {
            if (messages) {
                messages->append(QString("Failed to register with %1: %2").arg(agent.name, errorMsg));
            }
        }
    }

    return count;
}

int AgentConfigManager::unregisterAll(QStringList *messages)
{
    int count = 0;
    const AgentType types[] = {AGENT_CLAUDE, AGENT_GEMINI};

    for (AgentType type : types) {
        QString errorMsg;
        if (unregisterAgent(type, &errorMsg)) {
            count++;
            if (messages)
                messages->append(QString("Unregistered from %1").arg(agentName(type)));
        } else {
            if (messages)
                messages->append(QString("Failed to unregister from %1: %2").arg(agentName(type), errorMsg));
        }
    }

    return count;
}
