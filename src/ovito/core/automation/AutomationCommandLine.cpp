// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationCommandLine.h>
#include <ovito/core/automation/AutomationContract.h>
#include <ovito/core/automation/AutomationSessionDescriptor.h>
#include <ovito/core/automation/transport/AutomationLocalClient.h>

#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>

namespace Ovito {

namespace {

/// The exit code meanings of this mode; see the class comment.
enum ExitCode {
    ExitOk = 0,
    ExitRefused = 1,
    ExitUnavailable = 2
};

/// Writes one JSON object per line, so that a caller can read the answer of a single verb without a parser for
/// pretty-printed JSON. Human-readable output goes to the same stream otherwise.
void writeJson(QTextStream& stream, const QVariantMap& answer)
{
    stream << QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(answer)).toJson(QJsonDocument::Compact)) << Qt::endl;
}

/// Formats a value the way the human-readable output shows it: numbers and strings as they are, everything else as
/// compact JSON, so that a structured value is never printed as an empty QVariant.
QString humanValue(const QVariant& value)
{
    if(value.typeId() == QMetaType::QString)
        return value.toString();
    switch(value.typeId()) {
        case QMetaType::Bool:
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::UInt:
        case QMetaType::ULongLong:
            return value.toString();
        default:
            break;
    }
    if(value.metaType().id() == QMetaType::Double)
        return QString::number(value.toDouble());
    const QJsonValue json = value.toJsonValue();
    if(json.isNull())
        return QStringLiteral("<%1>").arg(QString::fromLatin1(value.typeName()));
    return QString::fromUtf8(QJsonDocument::fromVariant(json.toVariant()).toJson(QJsonDocument::Compact));
}

/// Prints the failure of one exchange as the answer of the verb.
int printFailure(QTextStream& out, QTextStream& err, bool json, const QString& verb, const QString& code, const QString& message, const QVariantMap& details = {})
{
    if(json) {
        QVariantMap error;
        error.insert(QStringLiteral("code"), code);
        error.insert(QStringLiteral("message"), message);
        if(!details.isEmpty())
            error.insert(QStringLiteral("details"), details);
        writeJson(out, QVariantMap{
                           { QStringLiteral("ok"), false },
                           { QStringLiteral("verb"), verb },
                           { QStringLiteral("error"), error }
                       });
    }
    else {
        err << QStringLiteral("error: %1: %2").arg(code, message) << Qt::endl;
    }
    return ExitRefused;
}

/// Prints the failure to reach a session at all, which is a different exit code from a refused request.
int printUnavailable(QTextStream& out, QTextStream& err, bool json, const QString& verb, const QString& message, const QStringList& problems = {})
{
    if(json) {
        QVariantMap answer;
        answer.insert(QStringLiteral("ok"), false);
        answer.insert(QStringLiteral("verb"), verb);
        answer.insert(QStringLiteral("error"), QVariantMap{
                                                 { QStringLiteral("code"), QStringLiteral("no_session") },
                                                 { QStringLiteral("message"), message }
                                             });
        if(!problems.isEmpty())
            answer.insert(QStringLiteral("problems"), problems);
        writeJson(out, answer);
    }
    else {
        err << QStringLiteral("error: %1").arg(message) << Qt::endl;
        for(const QString& problem : problems)
            err << QStringLiteral("  %1").arg(problem) << Qt::endl;
    }
    return ExitUnavailable;
}

/// The one discovered session the command line talks to: the one the caller named, or the newest live one.
bool selectSession(const QCommandLineParser& parser, AutomationSessionDescriptor& descriptor, QStringList& problems, QString& error)
{
    const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);
    if(sessions.empty()) {
        error = QStringLiteral("No running OVITO session was found. A workbench publishes a session only when it was "
                               "started with the '--automation-serve' option.");
        return false;
    }
    const QString requested = parser.value(QStringLiteral("session"));
    if(requested.isEmpty()) {
        descriptor = sessions.front();
        return true;
    }
    for(const AutomationSessionDescriptor& session : sessions) {
        if(session.sessionId() == requested || session.endpoint() == requested || session.displayLabel().contains(requested)) {
            descriptor = session;
            return true;
        }
    }
    error = QStringLiteral("No running session matches '%1'.").arg(requested);
    return false;
}

/// The human-readable summary of a `status` answer: what the session is, and what it is working on.
void printStatus(QTextStream& out, const QVariantMap& session, const QVariantMap& tasks, const QVariantMap& viewports, const QVariantMap& selection)
{
    out << QStringLiteral("session %1").arg(session.value(QStringLiteral("sessionFilePath")).toString().isEmpty()
                                               ? QStringLiteral("(no file)")
                                               : session.value(QStringLiteral("sessionFilePath")).toString()) << Qt::endl;
    out << QStringLiteral("  revision          %1").arg(session.value(QStringLiteral("revision")).toULongLong()) << Qt::endl;
    out << QStringLiteral("  objects           %1").arg(session.value(QStringLiteral("objectCount")).toLongLong()) << Qt::endl;
    out << QStringLiteral("  scene nodes       %1").arg(session.value(QStringLiteral("sceneNodeCount")).toInt()) << Qt::endl;
    out << QStringLiteral("  current frame     %1").arg(session.value(QStringLiteral("currentFrame")).toInt()) << Qt::endl;
    out << QStringLiteral("  selected          %1%2")
               .arg(selection.value(QStringLiteral("count")).toInt())
               .arg(selection.value(QStringLiteral("unavailable")).toBool() ? QStringLiteral(" (not granted)") : QString()) << Qt::endl;
    out << QStringLiteral("  viewports         %1").arg(viewports.value(QStringLiteral("viewports")).toList().size()) << Qt::endl;
    out << QStringLiteral("  tasks             %1").arg(tasks.value(QStringLiteral("tasks")).toList().size()) << Qt::endl;
    for(const QVariant& entry : tasks.value(QStringLiteral("tasks")).toList()) {
        const QVariantMap task = entry.toMap();
        out << QStringLiteral("    %1 %2 %3%4")
                   .arg(task.value(QStringLiteral("id")).toString(),
                        task.value(QStringLiteral("state")).toString(),
                        QString::number(task.value(QStringLiteral("progress")).toDouble() * 100.0, 'f', 0),
                        task.value(QStringLiteral("progressText")).toString().isEmpty()
                            ? QString()
                            : QStringLiteral(" (%1)").arg(task.value(QStringLiteral("progressText")).toString()))
            << Qt::endl;
    }
}

/// The human-readable listing of the sessions a client can discover.
void printSessions(QTextStream& out, const QVector<AutomationSessionDescriptor>& sessions, const QStringList& problems)
{
    if(sessions.empty())
        out << QStringLiteral("no running session") << Qt::endl;
    for(const AutomationSessionDescriptor& session : sessions)
        out << session.displayLabel() << Qt::endl;
    for(const QString& problem : problems)
        out << QStringLiteral("unusable: %1").arg(problem) << Qt::endl;
}

/// The human-readable summary of an object description: the shape every kind shares.
void printObject(QTextStream& out, const QVariantMap& object)
{
    out << QStringLiteral("%1  %2  %3").arg(object.value(QStringLiteral("id")).toString(),
                                            object.value(QStringLiteral("kind")).toString(),
                                            object.value(QStringLiteral("title")).toString()) << Qt::endl;
    for(auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString key = it.key();
        // The three fields above are already printed, and a nested parameter list is printed below.
        if(key == QStringLiteral("id") || key == QStringLiteral("kind") || key == QStringLiteral("title") || key == QStringLiteral("properties"))
            continue;
        if(it.value().metaType().id() == QMetaType::QVariantMap) {
            for(auto nested = it.value().toMap().constBegin(); nested != it.value().toMap().constEnd(); ++nested)
                out << QStringLiteral("  %1.%2 = %3").arg(key, nested.key(), humanValue(nested.value())) << Qt::endl;
        }
        else {
            out << QStringLiteral("  %1 = %2").arg(key, humanValue(it.value())) << Qt::endl;
        }
    }
    for(const QVariant& entry : object.value(QStringLiteral("properties")).toList()) {
        const QVariantMap property = entry.toMap();
        out << QStringLiteral("  %1 = %2  (%3)").arg(property.value(QStringLiteral("name")).toString(),
                                                      humanValue(property.value(QStringLiteral("value"))),
                                                      property.value(QStringLiteral("id")).toString()) << Qt::endl;
    }
}

}   // End of namespace

/******************************************************************************
* The verbs and the options of the automation mode.
******************************************************************************/
QStringList AutomationCommandLine::verbs()
{
    return {
        QStringLiteral("list"),
        QStringLiteral("status"),
        QStringLiteral("describe"),
        QStringLiteral("snapshot"),
        QStringLiteral("events")
    };
}

QStringList AutomationCommandLine::optionNames()
{
    return {
        QStringLiteral("automation"),
        QStringLiteral("json"),
        QStringLiteral("session"),
        QStringLiteral("since"),
        QStringLiteral("limit"),
        QStringLiteral("max-nodes")
    };
}

QString AutomationCommandLine::clientName()
{
    // The session records who asked, and the process id is what makes two invocations of the same script tellable
    // apart in the activity log; the path of the script is deliberately not part of it.
    return QStringLiteral("ovito-cli:%1").arg(QCoreApplication::applicationPid());
}

QStringList AutomationCommandLine::requestedCapabilities()
{
    // Read capabilities only: a client that connects to look around must not be able to change anything even if the
    // session would allow it (see the class comment and AUTOMATION_CONTRACTS.md).
    return {
        AutomationContract::capabilityName(AutomationContract::Capability::SessionRead),
        AutomationContract::capabilityName(AutomationContract::Capability::SceneRead),
        AutomationContract::capabilityName(AutomationContract::Capability::SelectionRead),
        AutomationContract::capabilityName(AutomationContract::Capability::FileRead)
    };
}

/******************************************************************************
* Runs one invocation and returns the process' exit code.
******************************************************************************/
int AutomationCommandLine::run(const QCommandLineParser& parser, const QStringList& arguments, QTextStream& out, QTextStream& err)
{
    const bool json = parser.isSet(QStringLiteral("json"));
    const QString verb = arguments.value(0);

    if(verb.isEmpty()) {
        err << QStringLiteral("error: usage: ovito --automation <%1> [--json] [--session <id>]")
                   .arg(verbs().join(QStringLiteral("|"))) << Qt::endl;
        return ExitUnavailable;
    }
    if(verb == QStringLiteral("list")) {
        // Listing never connects: the point of the discovery convention is that a client can see what is running
        // without disturbing it, and that includes a session whose socket does not answer.
        QStringList problems;
        const QVector<AutomationSessionDescriptor> sessions = AutomationLocalClient::discover(&problems);
        if(json) {
            QVariantList listed;
            for(const AutomationSessionDescriptor& session : sessions)
                listed.push_back(session.toJson());
            writeJson(out, QVariantMap{
                               { QStringLiteral("ok"), true },
                               { QStringLiteral("verb"), verb },
                               { QStringLiteral("sessions"), listed },
                               { QStringLiteral("problems"), problems }
                           });
        }
        else {
            printSessions(out, sessions, problems);
        }
        return ExitOk;
    }
    if(!verbs().contains(verb))
        return printUnavailable(out, err, json, verb, QStringLiteral("'%1' is not an automation verb of this build (%2).").arg(verb, verbs().join(QStringLiteral(", "))));
    // What the verb itself needs is checked before a session is looked for: a wrong invocation is a wrong invocation
    // whether or not something is running.
    if(verb == QStringLiteral("describe") && arguments.size() < 2)
        return printUnavailable(out, err, json, verb, QStringLiteral("usage: ovito --automation describe <object-id> [--json]"));

    // Every other verb talks to one session.
    AutomationSessionDescriptor descriptor;
    QStringList problems;
    QString error;
    if(!selectSession(parser, descriptor, problems, error))
        return printUnavailable(out, err, json, verb, error, problems);

    AutomationLocalClient client;
    if(!client.connectToEndpoint(descriptor.endpoint(), &error))
        return printUnavailable(out, err, json, verb, QStringLiteral("The session %1 does not answer on its local socket: %2").arg(descriptor.displayLabel(), error));

    // Identify the client, and report what the session granted it: a client that is silently given less than it asked
    // for is a client that will misreport, so the answer names both.
    const AutomationLocalClient::Reply hello = client.hello(clientName(), requestedCapabilities());
    if(!hello.transportOk)
        return printUnavailable(out, err, json, verb, QStringLiteral("The session rejected the connection: %1").arg(hello.errorMessage));

    auto refusedCapabilities = [&]() -> QStringList {
        return hello.result.value(QStringLiteral("refusedCapabilities")).toStringList();
    };
    auto contractError = [&](const AutomationLocalClient::Reply& reply) {
        const QVariantMap result = reply.result;
        return printFailure(out, err, json, verb,
                            result.value(QStringLiteral("error")).toMap().value(QStringLiteral("code")).toString(),
                            result.value(QStringLiteral("error")).toMap().value(QStringLiteral("message")).toString(),
                            result.value(QStringLiteral("error")).toMap().value(QStringLiteral("details")).toMap());
    };

    if(verb == QStringLiteral("status")) {
        const AutomationLocalClient::Reply session = client.dispatch(QStringLiteral("session.describe"));
        if(!session.transportOk || !session.result.value(QStringLiteral("ok")).toBool())
            return session.transportOk ? contractError(session) : printUnavailable(out, err, json, verb, session.errorMessage);
        const AutomationLocalClient::Reply viewports = client.dispatch(QStringLiteral("viewport.list"));
        // The selection and the tasks are reported as far as the session grants them; a session that refuses them is
        // still worth a status answer, so their absence is not an error here.
        const AutomationLocalClient::Reply selection = client.dispatch(QStringLiteral("selection.describe"));
        const int limit = parser.isSet(QStringLiteral("limit")) ? parser.value(QStringLiteral("limit")).toInt() : 10;
        const AutomationLocalClient::Reply tasks = client.dispatch(QStringLiteral("task.list"), { { QStringLiteral("limit"), limit } });
        const QVariantMap status = session.result.value(QStringLiteral("data")).toMap();
        const QVariantMap viewportData = viewports.transportOk && viewports.result.value(QStringLiteral("ok")).toBool() ? viewports.result.value(QStringLiteral("data")).toMap() : QVariantMap{};
        // A selection the session refuses (the capability is optional) is reported as such rather than as an empty
        // selection: a caller that reads only the payload must be able to tell "nothing is selected" from "not allowed".
        QVariantMap selectionData = selection.result.value(QStringLiteral("data")).toMap();
        if(!selection.transportOk || !selection.result.value(QStringLiteral("ok")).toBool()) {
            selectionData.insert(QStringLiteral("count"), 0);
            selectionData.insert(QStringLiteral("unavailable"), true);
            const QVariantMap error = selection.result.value(QStringLiteral("error")).toMap();
            if(!error.isEmpty())
                selectionData.insert(QStringLiteral("error"), error);
        }
        const QVariantMap taskData = tasks.transportOk && tasks.result.value(QStringLiteral("ok")).toBool() ? tasks.result.value(QStringLiteral("data")).toMap() : QVariantMap{};
        if(json) {
            writeJson(out, QVariantMap{
                               { QStringLiteral("ok"), true },
                               { QStringLiteral("verb"), verb },
                               { QStringLiteral("sessionId"), descriptor.sessionId() },
                               { QStringLiteral("processId"), QVariant::fromValue<qlonglong>(descriptor.processId()) },
                               { QStringLiteral("contractVersion"), hello.result.value(QStringLiteral("contractVersion")).toString() },
                               { QStringLiteral("grantedCapabilities"), hello.result.value(QStringLiteral("grantedCapabilities")).toStringList() },
                               { QStringLiteral("refusedCapabilities"), refusedCapabilities() },
                               { QStringLiteral("session"), status },
                               { QStringLiteral("viewports"), viewportData },
                               { QStringLiteral("selection"), selectionData },
                               { QStringLiteral("tasks"), taskData }
                           });
        }
        else {
            out << QStringLiteral("client %1  granted %2%3")
                       .arg(client.clientId(),
                            hello.result.value(QStringLiteral("grantedCapabilities")).toStringList().join(QStringLiteral(", ")),
                            refusedCapabilities().isEmpty() ? QString() : QStringLiteral("  refused %1").arg(refusedCapabilities().join(QStringLiteral(", "))))
                << Qt::endl;
            printStatus(out, status, taskData, viewportData, selectionData);
        }
        return ExitOk;
    }

    if(verb == QStringLiteral("describe")) {
        const AutomationLocalClient::Reply reply = client.dispatch(QStringLiteral("object.describe"), { { QStringLiteral("objectId"), arguments.at(1) } });
        if(!reply.transportOk)
            return printUnavailable(out, err, json, verb, reply.errorMessage);
        if(!reply.result.value(QStringLiteral("ok")).toBool())
            return contractError(reply);
        const QVariantMap object = reply.result.value(QStringLiteral("data")).toMap();
        if(json)
            writeJson(out, QVariantMap{ { QStringLiteral("ok"), true }, { QStringLiteral("verb"), verb }, { QStringLiteral("object"), object } });
        else
            printObject(out, object);
        return ExitOk;
    }

    if(verb == QStringLiteral("snapshot")) {
        const int maximumNodes = parser.isSet(QStringLiteral("max-nodes")) ? parser.value(QStringLiteral("max-nodes")).toInt() : 0;
        const AutomationLocalClient::Reply reply = client.snapshot(maximumNodes);
        if(!reply.transportOk)
            return printUnavailable(out, err, json, verb, reply.errorMessage);
        if(json) {
            writeJson(out, QVariantMap{ { QStringLiteral("ok"), true }, { QStringLiteral("verb"), verb }, { QStringLiteral("snapshot"), reply.result } });
        }
        else {
            const QVariantMap scene = reply.result.value(QStringLiteral("scene")).toMap();
            const QVariantMap session = reply.result.value(QStringLiteral("session")).toMap();
            printStatus(out, session, reply.result.value(QStringLiteral("tasks")).toMap(), reply.result.value(QStringLiteral("viewports")).toMap(), reply.result.value(QStringLiteral("selection")).toMap());
            out << QStringLiteral("  nodes             %1%2")
                       .arg(scene.value(QStringLiteral("nodeCount")).toInt())
                       .arg(scene.value(QStringLiteral("truncated")).toBool() ? QStringLiteral(" (truncated)") : QString()) << Qt::endl;
            for(const QVariant& entry : scene.value(QStringLiteral("nodes")).toList()) {
                const QVariantMap node = entry.toMap();
                out << QStringLiteral("    %1  %2  %3").arg(node.value(QStringLiteral("id")).toString(),
                                                            node.value(QStringLiteral("title")).toString(),
                                                            node.value(QStringLiteral("pipelineId")).toString()) << Qt::endl;
            }
        }
        return ExitOk;
    }

    if(verb == QStringLiteral("events")) {
        QVariantMap eventArguments;
        if(parser.isSet(QStringLiteral("since")))
            eventArguments.insert(QStringLiteral("since"), parser.value(QStringLiteral("since")).toInt());
        if(parser.isSet(QStringLiteral("limit")))
            eventArguments.insert(QStringLiteral("limit"), parser.value(QStringLiteral("limit")).toInt());
        const AutomationLocalClient::Reply reply = client.dispatch(QStringLiteral("event.list"), eventArguments);
        if(!reply.transportOk)
            return printUnavailable(out, err, json, verb, reply.errorMessage);
        if(!reply.result.value(QStringLiteral("ok")).toBool())
            return contractError(reply);
        const QVariantMap events = reply.result.value(QStringLiteral("data")).toMap();
        if(json) {
            writeJson(out, QVariantMap{
                               { QStringLiteral("ok"), true },
                               { QStringLiteral("verb"), verb },
                               { QStringLiteral("events"), events }
                           });
        }
        else {
            for(const QVariant& entry : events.value(QStringLiteral("events")).toList()) {
                const QVariantMap event = entry.toMap();
                out << QStringLiteral("%1  %2  %3  %4").arg(event.value(QStringLiteral("sequence")).toString(),
                                                            event.value(QStringLiteral("kind")).toString(),
                                                            event.value(QStringLiteral("origin")).toString(),
                                                            event.value(QStringLiteral("summary")).toString()) << Qt::endl;
            }
        }
        return ExitOk;
    }

    // The verb list and this dispatch must not drift apart: a verb that is offered but not handled here would
    // otherwise be answered with the request of the last block above.
    err << QStringLiteral("error: the verb '%1' is offered by this build but is not implemented.").arg(verb) << Qt::endl;
    return ExitUnavailable;
}

}   // End of namespace
