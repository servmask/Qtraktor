#include "mcpserver.h"
#include "agentconfig.h"
#include "backupfile.h"
#include "cryptoutils.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cstdio>

static const QString MCP_PROTOCOL_VERSION = "2024-11-05";
static const QString SERVER_NAME = "traktor";
static const QString SERVER_VERSION = "1.0.0";

// ── JSON-RPC helpers ────────────────────────────────────────────────────────

static QJsonObject makeResponse(const QJsonValue &id, const QJsonObject &result)
{
    QJsonObject resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["result"] = result;
    return resp;
}

static QJsonObject makeError(const QJsonValue &id, int code, const QString &message)
{
    QJsonObject err;
    err["code"] = code;
    err["message"] = message;

    QJsonObject resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["error"] = err;
    return resp;
}

static QJsonObject makeToolResult(const QString &text)
{
    QJsonObject content;
    content["type"] = "text";
    content["text"] = text;

    QJsonArray contentArray;
    contentArray.append(content);

    QJsonObject result;
    result["content"] = contentArray;
    return result;
}

static QJsonObject makeToolError(const QString &text)
{
    QJsonObject content;
    content["type"] = "text";
    content["text"] = text;

    QJsonArray contentArray;
    contentArray.append(content);

    QJsonObject result;
    result["content"] = contentArray;
    result["isError"] = true;
    return result;
}

// ── Tool schemas ────────────────────────────────────────────────────────────

static QJsonArray buildToolSchemas()
{
    QJsonArray tools;

    auto makeSchema = [](const QString &name, const QString &desc, const QJsonObject &props,
                         const QJsonArray &required) {
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        schema["required"] = required;

        QJsonObject tool;
        tool["name"] = name;
        tool["description"] = desc;
        tool["inputSchema"] = schema;
        return tool;
    };

    QJsonObject archiveProp;
    archiveProp["type"] = "string";
    archiveProp["description"] = "Path to the .wpress archive file";

    QJsonObject passwordProp;
    passwordProp["type"] = "string";
    passwordProp["description"] = "Password for encrypted archives (optional)";

    // list
    {
        QJsonObject props;
        props["archive"] = archiveProp;
        props["password"] = passwordProp;
        tools.append(makeSchema("list",
                                "List contents of a .wpress backup archive. Returns file paths, sizes, "
                                "and modification times as a JSON array.",
                                props, QJsonArray() << "archive"));
    }
    // info
    {
        QJsonObject props;
        props["archive"] = archiveProp;
        props["password"] = passwordProp;
        tools.append(
            makeSchema("info",
                       "Show metadata of a .wpress archive: format version, encryption, compression, file count, "
                       "total size.",
                       props, QJsonArray() << "archive"));
    }
    // extract
    {
        QJsonObject destProp;
        destProp["type"] = "string";
        destProp["description"] = "Directory to extract into (default: current directory)";

        QJsonObject props;
        props["archive"] = archiveProp;
        props["destination"] = destProp;
        props["password"] = passwordProp;
        tools.append(makeSchema("extract",
                                "Extract all files from a .wpress backup archive to a destination directory.", props,
                                QJsonArray() << "archive"));
    }
    // cat
    {
        QJsonObject pathProp;
        pathProp["type"] = "string";
        pathProp["description"] = "Path of the file inside the archive (e.g., wp-config.php)";

        QJsonObject props;
        props["archive"] = archiveProp;
        props["path"] = pathProp;
        props["password"] = passwordProp;
        QJsonArray catRequired;
        catRequired << "archive" << "path";
        tools.append(makeSchema("cat",
                                "Read a single file from inside a .wpress archive without extracting the entire "
                                "archive. Returns the file content as text. For files larger than 10 MB, consider "
                                "using extract instead.",
                                props, catRequired));
    }
    // verify
    {
        QJsonObject props;
        props["archive"] = archiveProp;
        props["password"] = passwordProp;
        tools.append(makeSchema(
            "verify",
            "Verify integrity of a .wpress archive by checking CRC32 checksums. Returns per-file verification results.",
            props, QJsonArray() << "archive"));
    }

    return tools;
}

// ── Archive helper ──────────────────────────────────────────────────────────

static BackupFile *openArchive(const QString &path, const QString &password, QString &errorMsg)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isReadable()) {
        errorMsg = QString("Cannot read file: %1").arg(path);
        return nullptr;
    }

    BackupFile *bf = new BackupFile(path, password);
    if (!bf->open(QIODevice::ReadOnly)) {
        errorMsg = QString("Cannot open file: %1").arg(path);
        delete bf;
        return nullptr;
    }

    if (!bf->isValid()) {
        errorMsg = "Archive is corrupted or not a valid .wpress file";
        bf->close();
        delete bf;
        return nullptr;
    }

    bf->ensureConfigLoaded();

    if (bf->isEncryptedFile() && password.isEmpty()) {
        errorMsg = "Archive is encrypted - provide a password";
        bf->close();
        delete bf;
        return nullptr;
    }

    bf->setConfig(bf->isEncryptedFile(), bf->getCompressionType());
    return bf;
}

// ── Tool handlers ───────────────────────────────────────────────────────────

static QJsonObject handleList(const QJsonObject &args)
{
    const QString archive = args["archive"].toString();
    const QString password = args["password"].toString();

    QString errorMsg;
    BackupFile *bf = openArchive(archive, password, errorMsg);
    if (!bf) {
        return makeToolError(errorMsg);
    }

    QJsonArray entries;
    bool ok = bf->iterateHeaders([&](const BackupFile::HeaderInfo &info) {
        QJsonObject entry;
        entry["path"] = BackupFile::normalizePath(info.filePath, info.fileName);
        entry["size"] = info.fileSize;
        entry["mtime"] = info.mtime.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(info.mtime);
        if (!info.crc32.isEmpty()) {
            entry["crc32"] = info.crc32;
        }
        entries.append(entry);
        return true;
    });

    bf->close();
    delete bf;

    if (!ok) {
        return makeToolError("Failed to read archive headers");
    }

    return makeToolResult(QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

static QJsonObject handleInfo(const QJsonObject &args)
{
    const QString archive = args["archive"].toString();
    const QString password = args["password"].toString();

    QString errorMsg;
    BackupFile *bf = openArchive(archive, password, errorMsg);
    if (!bf) {
        return makeToolError(errorMsg);
    }

    QJsonObject info = bf->getArchiveInfo();
    bf->close();
    delete bf;

    return makeToolResult(QJsonDocument(info).toJson(QJsonDocument::Compact));
}

static QJsonObject handleExtract(const QJsonObject &args)
{
    const QString archive = args["archive"].toString();
    const QString password = args["password"].toString();
    QString destPath = args["destination"].toString();
    if (destPath.isEmpty()) {
        destPath = QDir::currentPath();
    }

    QString errorMsg;
    BackupFile *bf = openArchive(archive, password, errorMsg);
    if (!bf) {
        return makeToolError(errorMsg);
    }

    QFileInfo archiveInfo(archive);
    QDir extractTo(destPath + "/" + archiveInfo.baseName());

    if (!QDir().mkpath(extractTo.path())) {
        bf->close();
        delete bf;
        return makeToolError(QString("Cannot create directory: %1").arg(extractTo.path()));
    }

    const bool ok = bf->extract(extractTo);
    bf->close();
    delete bf;

    if (!ok) {
        return makeToolError("Extraction failed");
    }

    QJsonObject result;
    result["status"] = "success";
    result["destination"] = extractTo.path();
    return makeToolResult(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

static QJsonObject handleCat(const QJsonObject &args)
{
    const QString archive = args["archive"].toString();
    const QString filePath = args["path"].toString();
    const QString password = args["password"].toString();

    if (filePath.isEmpty()) {
        return makeToolError("Missing required parameter: path");
    }

    QString errorMsg;
    BackupFile *bf = openArchive(archive, password, errorMsg);
    if (!bf) {
        return makeToolError(errorMsg);
    }

    QBuffer output;
    output.open(QIODevice::WriteOnly);

    const bool ok = bf->extractSingleFile(filePath, &output);
    bf->close();
    delete bf;

    if (!ok) {
        return makeToolError(QString("File not found in archive: %1").arg(filePath));
    }

    return makeToolResult(QString::fromUtf8(output.data()));
}

static QJsonObject handleVerify(const QJsonObject &args)
{
    const QString archive = args["archive"].toString();
    const QString password = args["password"].toString();

    QString errorMsg;
    BackupFile *bf = openArchive(archive, password, errorMsg);
    if (!bf) {
        return makeToolError(errorMsg);
    }

    const bool isV2 = bf->isV2Format();
    QJsonArray results;
    bool allPassed = true;

    bool iterOk = bf->iterateHeaders([&](const BackupFile::HeaderInfo &info) {
        const QString path = BackupFile::normalizePath(info.filePath, info.fileName);
        QJsonObject entry;
        entry["path"] = path;

        if (isV2 && !info.crc32.isEmpty()) {
            CrcDevice crcSink;
            const bool isCompressed =
                !CryptoUtils::isConfigFile(info.fileName) && bf->getCompressionType() != COMPRESSION_NONE;

            QString processError;
            bool streamOk;

            if (bf->isEncryptedFile() && !password.isEmpty()) {
                streamOk = CryptoUtils::processFileContentWithPasswordStreaming(
                    bf, info.fileSize, &crcSink, isCompressed, info.fileName, password, bf->getCompressionType(),
                    &processError);
            } else {
                streamOk = CryptoUtils::processFileContentStreaming(
                    bf, info.fileSize, &crcSink, isCompressed, info.fileName, bf->getCompressionType(), &processError);
            }

            const QString actualCrc = crcSink.result();

            if (!streamOk) {
                entry["status"] = "error";
                if (!processError.isEmpty()) {
                    entry["error"] = processError;
                }
                allPassed = false;
            } else if (actualCrc == info.crc32) {
                entry["status"] = "pass";
            } else {
                entry["status"] = "fail";
                entry["expectedCrc"] = info.crc32;
                entry["actualCrc"] = actualCrc;
                allPassed = false;
            }
        } else {
            entry["status"] = "unchecked";
        }

        results.append(entry);
        return true;
    });

    bf->close();
    delete bf;

    if (!iterOk) {
        return makeToolError("Failed to read archive for verification");
    }

    QJsonObject summary;
    summary["allPassed"] = allPassed;
    summary["files"] = results;
    return makeToolResult(QJsonDocument(summary).toJson(QJsonDocument::Compact));
}

// ── Request dispatcher ──────────────────────────────────────────────────────

static QJsonObject dispatch(const QJsonObject &request)
{
    const QString method = request["method"].toString();
    const QJsonValue id = request["id"];

    // Initialize
    if (method == "initialize") {
        QJsonObject caps;
        caps["tools"] = QJsonObject();

        QJsonObject serverInfo;
        serverInfo["name"] = SERVER_NAME;
        serverInfo["version"] = SERVER_VERSION;

        QJsonObject result;
        result["protocolVersion"] = MCP_PROTOCOL_VERSION;
        result["capabilities"] = caps;
        result["serverInfo"] = serverInfo;

        return makeResponse(id, result);
    }

    // Notifications have no id - don't respond
    if (method == "notifications/initialized") {
        return QJsonObject(); // empty = no response
    }

    // Tools list
    if (method == "tools/list") {
        QJsonObject result;
        result["tools"] = buildToolSchemas();
        return makeResponse(id, result);
    }

    // Tools call
    if (method == "tools/call") {
        const QJsonObject params = request["params"].toObject();
        const QString toolName = params["name"].toString();
        const QJsonObject args = params["arguments"].toObject();

        QJsonObject toolResult;
        if (toolName == "list") {
            toolResult = handleList(args);
        } else if (toolName == "info") {
            toolResult = handleInfo(args);
        } else if (toolName == "extract") {
            toolResult = handleExtract(args);
        } else if (toolName == "cat") {
            toolResult = handleCat(args);
        } else if (toolName == "verify") {
            toolResult = handleVerify(args);
        } else {
            return makeError(id, -32602, QString("Unknown tool: %1").arg(toolName));
        }

        return makeResponse(id, toolResult);
    }

    // Unknown method
    return makeError(id, -32601, QString("Method not found: %1").arg(method));
}

// ── Main MCP loop ───────────────────────────────────────────────────────────

// ── MCP sub-commands (register/unregister/status) ───────────────────────────

static int cmdMcpRegister(const QStringList &args)
{
    bool jsonMode = args.contains("--json");
    AgentConfigManager mgr;
    QStringList messages;
    int count = mgr.registerAllDetected(&messages);

    if (jsonMode) {
        QJsonObject result;
        result["registered"] = count;
        QJsonArray msgArray;
        for (const QString &msg : messages)
            msgArray.append(msg);
        result["messages"] = msgArray;
        fprintf(stdout, "%s\n", QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    } else {
        for (const QString &msg : messages)
            fprintf(stdout, "%s\n", msg.toLocal8Bit().constData());
        fprintf(stdout, "%d agent(s) registered.\n", count);
    }
    fflush(stdout);
    return 0;
}

static int cmdMcpUnregister()
{
    AgentConfigManager mgr;
    QStringList messages;
    mgr.unregisterAll(&messages);

    for (const QString &msg : messages)
        fprintf(stdout, "%s\n", msg.toLocal8Bit().constData());
    fflush(stdout);
    return 0;
}

static int cmdMcpStatus(const QStringList &args)
{
    bool jsonMode = args.contains("--json");
    AgentConfigManager mgr;
    QList<AgentInfo> agents = mgr.detectAgents();

    if (jsonMode) {
        QJsonArray arr;
        for (const AgentInfo &a : agents) {
            QJsonObject obj;
            obj["name"] = a.name;
            obj["configPath"] = a.configPath;
            obj["detected"] = a.detected;
            obj["registered"] = a.registered;
            arr.append(obj);
        }
        fprintf(stdout, "%s\n", QJsonDocument(arr).toJson(QJsonDocument::Compact).constData());
    } else {
        fprintf(stdout, "%-15s %-10s %-10s\n", "Agent", "Detected", "Registered");
        for (const AgentInfo &a : agents) {
            fprintf(stdout, "%-15s %-10s %-10s\n", a.name.toLocal8Bit().constData(), a.detected ? "yes" : "no",
                    a.detected ? (a.registered ? "yes" : "no") : "-");
        }
    }
    fflush(stdout);
    return 0;
}

// ── Main MCP entry point ────────────────────────────────────────────────────

int cmdMcp()
{
    // Check for sub-subcommands: traktor mcp register/unregister/status
    QStringList args = QCoreApplication::arguments();
    if (args.size() >= 3) {
        const QString sub = args.at(2);
        if (sub == "register")
            return cmdMcpRegister(args);
        if (sub == "unregister")
            return cmdMcpUnregister();
        if (sub == "status")
            return cmdMcpStatus(args);
    }

    // No sub-subcommand: start MCP JSON-RPC server
    QTextStream in(stdin);
    QTextStream out(stdout);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            QJsonObject resp = makeError(QJsonValue::Null, -32700, "Parse error");
            out << QJsonDocument(resp).toJson(QJsonDocument::Compact) << "\n";
            out.flush();
            continue;
        }

        const QJsonObject request = doc.object();
        const QJsonObject response = dispatch(request);

        // Notifications produce an empty response - don't send anything
        if (response.isEmpty()) {
            continue;
        }

        out << QJsonDocument(response).toJson(QJsonDocument::Compact) << "\n";
        out.flush();
    }

    return 0;
}
