// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

#include <QString>
#include <QStringList>
#include <QVariantMap>

class QCommandLineParser;
class QTextStream;

namespace Ovito {

/**
 * \brief The `ovito --automation <verb>` command line: a read-only client of a running workbench.
 *
 * Phase 3's deliverable 7 asks for the first automation client that is usable from the outside, and this is it - a mode
 * of the shipped `ovito` binary rather than a second executable (audit decision D60). A second program would duplicate
 * the argument parsing, the session discovery and the error vocabulary of the running process, and would drift.
 *
 * What the command line can do is what the contract allows a read-only client to do, and nothing more:
 *
 * - `list` shows the sessions a local client can discover, without connecting to any of them.
 * - `status` answers what the newest session is: its revision, its file, its scene, its viewports and what it is
 *   working on.
 * - `describe <object-id>` answers one object - a scene node, pipeline, modifier, viewport or one of its parameters -
 *   by the ID a previous answer reported.
 * - `snapshot` answers the bounded composition a connecting client gets in one call.
 * - `events` answers the session's recent activity without their raw inputs.
 *
 * Every verb prints human-readable text, or one JSON object with `--json` for a script. The capabilities it asks the
 * session for are the read capabilities; it never asks for one that permits a change, executes Python or writes a file,
 * and a session that refuses even the read capabilities is reported as such instead of being worked around.
 *
 * The exit code says what happened: 0 the verb answered, 1 the session refused the request or the exchange broke, 2 the
 * request could not be carried out at all (no running session was found, or the arguments are not a verb this build
 * knows). A stale descriptor of a session whose process is gone is one of those cases: the command line reports it and
 * leaves the file alone, because removing another process' discovery entry is not its decision (audit decision D50).
 */
class OVITO_CORE_EXPORT AutomationCommandLine
{
public:

    /// The verbs this build knows, in the order the help text lists them.
    static QStringList verbs();

    /// The options the automation mode adds to the command line: what a session is, and how much to report.
    static QStringList optionNames();

    /// Runs one invocation and returns the process' exit code. \a arguments are the parser's positional arguments: the
    /// verb and, for `describe`, the object ID.
    static int run(const QCommandLineParser& parser, const QStringList& arguments, QTextStream& out, QTextStream& err);

    /// The client name the session records for a command line invocation of this process.
    static QString clientName();

    /// The capabilities the command line asks for: the read capabilities and nothing else.
    static QStringList requestedCapabilities();

private:

    /// Formats and prints one answer, in the form the caller asked for.
    static void printAnswer(QTextStream& out, bool json, const QVariantMap& answer);

    /// Prints the session a verb answered about, and the operation's own error if there is one.
    static int printResult(QTextStream& out, QTextStream& err, bool json, const QString& verb, const QVariantMap& result);
};

}   // End of namespace
