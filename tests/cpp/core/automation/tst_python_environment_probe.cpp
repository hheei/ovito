// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \brief Tests of the Python environment probe, the runtime half of the package contract (Phase 2.6, deliverable 5).
 *
 * The probe is deliberately independent of the OVITO object model: it spawns an interpreter, reads one handshake and
 * applies the rules of PythonContract. That is what makes this suite runnable without an Application instance, an
 * ambient task or a GUI - the same way a CLI probe or a future environment selector uses it.
 *
 * The suite never requires a network or an installed `ovito` package: it uses the interpreter it finds for the
 * environment-level cases and writes its own fixture packages and fixture handshakes into a temporary directory for
 * everything that must be deterministic. A machine without a `python3` executable skips the cases that need one
 * instead of failing (see SKIP_WITHOUT_PYTHON below).
 */

#include <ovito/core/automation/python/PythonContract.h>
#include <ovito/core/automation/python/PythonEnvironmentProbe.h>
#include <ovito/core/automation/python/PythonHandshake.h>

#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QTemporaryDir>

using namespace Ovito;

/**
 * Skips the current test case on a machine that has no Python interpreter at all, and defines `interpreter` for the
 * case. The probe's own behaviour without an interpreter is covered by the cases that pass none, so a machine without
 * Python loses coverage of the environment rules rather than reporting failures it cannot help.
 */
#define SKIP_WITHOUT_PYTHON()                                                                                                    \
    const QString interpreter = PythonEnvironmentProbe::findInterpreter();                                                       \
    if(interpreter.isEmpty())                                                                                                    \
    QSKIP("This machine has no python3 executable on its path.")

class PythonEnvironmentProbeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase();

    // The wire types of the handshake: structure, forward compatibility and the feature vocabulary.
    void contract_declares_a_bounded_python_envelope();
    void handshake_round_trips();
    void handshake_keeps_features_of_a_newer_protocol();
    void handshake_rejects_malformed_messages();

    // The probe against the interpreter of this machine, which has no ovito package.
    void probe_finds_the_interpreter_of_this_machine();
    void probe_blocking_matches_the_asynchronous_interface();

    // The probe against fixture packages and fixture handshakes: one case per compatibility rule.
    void probe_accepts_a_compatible_package();
    void probe_reports_a_package_that_cannot_be_imported();
    void probe_reports_an_incompatible_protocol_version();
    void probe_reports_a_missing_feature();
    void probe_reports_an_unsupported_python();
    void probe_reports_an_interpreter_mismatch();

    // The failure modes of the process itself.
    void probe_reports_an_interpreter_that_does_not_exist();
    void probe_reports_an_interpreter_that_does_not_answer();
    void probe_reports_an_answer_that_is_not_a_handshake();
    void probe_reports_a_timeout();
    void probe_reports_a_missing_probe_script();
    void probe_reports_a_missing_configuration();
    void probe_can_be_cancelled();
    void probe_emits_exactly_one_verdict();

private:

    /// Writes a file below the temporary directory, creating the parent directories. Returns its path.
    QString writeFixture(const QString& relativePath, const QString& content);

    /**
     * Writes a fixture `ovito` package that answers the handshake with the given protocol version, version number and
     * features. `importError` makes the package raise while it is imported, which is the mixed-environment case.
     */
    QString writeFixturePackage(const QStringList& features, const QString& protocolVersion = QStringLiteral("1.0"),
                                const QString& version = QStringLiteral("0.1.0"),
                                const QString& importError = QString());

    /// A request for the fixture package, which needs a non-isolated interpreter so that PYTHONPATH applies.
    PythonProbeRequest requestForFixturePackage(const QString& interpreter, const QString& packageDirectory);

    /// A request that runs a fixture probe script instead of the built-in one.
    PythonProbeRequest requestForFixtureScript(const QString& interpreter, const QString& scriptPath);

    /**
     * Writes a probe script that prints the given handshake verbatim, so that a case tests a validation rule rather
     * than the behaviour of a real interpreter. `prologue` is printed before the handshake.
     */
    QString writeFixtureHandshakeScript(const QVariantMap& handshake, const QString& prologue = QString());

    QTemporaryDir _tempDirectory;
};

/******************************************************************************
* Prepares the temporary directory the fixtures live in.
******************************************************************************/
void PythonEnvironmentProbeTest::initTestCase()
{
    QVERIFY2(_tempDirectory.isValid(), "Cannot create a temporary directory for the probe fixtures.");
}

/******************************************************************************
* The envelope and the vocabulary itself.
******************************************************************************/
void PythonEnvironmentProbeTest::contract_declares_a_bounded_python_envelope()
{
    // The handshake name and the package name are part of the protocol.
    QCOMPARE(PythonContract::handshakeName(), QStringLiteral("ovito.automation.handshake"));
    QCOMPARE(PythonContract::packageName(), QStringLiteral("ovito"));
    QCOMPARE(PythonContract::protocolVersion(),
             QStringLiteral("%1.%2").arg(PythonContract::protocolVersionMajor).arg(PythonContract::protocolVersionMinor));

    // The supported interpreter range is bounded on both sides, and it is asked as a range, not as a set.
    QVERIFY(PythonContract::supportsPythonVersion(3, 12));
    QVERIFY(PythonContract::supportsPythonVersion(PythonContract::minimumPythonVersion().at(0), PythonContract::minimumPythonVersion().at(1)));
    QVERIFY(PythonContract::supportsPythonVersion(PythonContract::maximumPythonVersion().at(0), PythonContract::maximumPythonVersion().at(1)));
    QVERIFY(!PythonContract::supportsPythonVersion(3, PythonContract::minimumPythonVersion().at(1) - 1));
    QVERIFY(!PythonContract::supportsPythonVersion(3, PythonContract::maximumPythonVersion().at(1) + 1));
    QVERIFY(!PythonContract::supportsPythonVersion(2, 7));

    // Every feature has a unique wire name and survives the round trip through its name.
    const QVector<PythonContract::Feature> features = PythonContract::allFeatures();
    QVERIFY(features.size() >= 5);
    QSet<QString> names;
    for(PythonContract::Feature feature : features) {
        const QString name = PythonContract::featureName(feature);
        QVERIFY(!name.isEmpty());
        QVERIFY(!names.contains(name));
        names.insert(name);
        QCOMPARE(PythonContract::featureFromName(name).value_or(PythonContract::Feature::SchemaIntrospection), feature);
    }
    QVERIFY(!PythonContract::featureFromName(QStringLiteral("future.feature")).has_value());

    // The probe status names are unique and round-trip as well, and `compatible` is the first one - a caller that
    // switches on the status relies on the order of the checks, not on numbers.
    QCOMPARE(PythonContract::probeStatusName(PythonContract::ProbeStatus::Compatible), QStringLiteral("compatible"));
    QCOMPARE(PythonContract::probeStatusName(PythonContract::ProbeStatus::PackageMissing), QStringLiteral("package_missing"));
    const std::optional<PythonContract::ProbeStatus> featureMissing =
        PythonContract::probeStatusFromName(QStringLiteral("feature_missing"));
    QVERIFY(featureMissing.has_value());
    QCOMPARE(*featureMissing, PythonContract::ProbeStatus::FeatureMissing);
    QVERIFY(!PythonContract::probeStatusFromName(QStringLiteral("unknown_status")).has_value());

    // The default requirements are the first modifier contract: metadata, the in-place call, and its array transport.
    const QVector<PythonContract::Feature> required = PythonContract::defaultRequiredFeatures();
    QVERIFY(required.contains(PythonContract::Feature::SchemaIntrospection));
    QVERIFY(required.contains(PythonContract::Feature::FunctionInplace));
    QVERIFY(required.contains(PythonContract::Feature::ArrayBuffer));

    // This build knows where its probe script is (the test only makes sense in the source tree).
    const QString script = PythonEnvironmentProbe::defaultScriptFile();
    QVERIFY2(!script.isEmpty(), "The build did not bake in the probe script directory.");
    QVERIFY2(QFileInfo::exists(script), qPrintable(QStringLiteral("The probe script %1 does not exist.").arg(script)));
}

void PythonEnvironmentProbeTest::handshake_round_trips()
{
    PythonEnvironmentInfo environment;
    environment.setImplementation(QStringLiteral("cpython"))
        .setVersion(QStringLiteral("3.12.3"))
        .setVersionInfo(3, 12, 3)
        .setExecutable(QStringLiteral("/usr/bin/python3"))
        .setPrefix(QStringLiteral("/usr"))
        .setBasePrefix(QStringLiteral("/usr"))
        .setPlatform(QStringLiteral("linux"))
        .setMachine(QStringLiteral("x86_64"))
        .setArchitecture(QStringLiteral("64bit"))
        .setCacheTag(QStringLiteral("cpython-312"))
        .setIsolated(true);
    PythonPackageInfo package;
    package.setName(QStringLiteral("ovito"))
        .setFound(true)
        .setImported(true)
        .setVersion(QStringLiteral("0.1.0"))
        .setProtocolVersion(QStringLiteral("1.0"))
        .setModuleFile(QStringLiteral("/env/lib/python3.12/site-packages/ovito/__init__.py"))
        .setFeatures(PythonContract::defaultRequiredFeatures());
    PythonHandshake handshake;
    handshake.setProtocolName(PythonContract::handshakeName())
        .setProtocolVersion(QStringLiteral("1.0"))
        .setEnvironment(environment)
        .setPackage(package)
        .setFeatures(package.features());

    QVERIFY(handshake.isValid());
    QCOMPARE(handshake.protocolVersionMajor(), 1);
    QCOMPARE(handshake.protocolVersionMinor(), 0);
    QCOMPARE(environment.displayName(), QStringLiteral("CPython 3.12.3"));

    const QVariantMap json = handshake.toJson();
    QString error;
    const std::optional<PythonHandshake> parsed = PythonHandshake::fromJson(json, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(parsed->protocolName(), PythonContract::handshakeName());
    QCOMPARE(parsed->protocolVersion(), QStringLiteral("1.0"));
    QCOMPARE(parsed->environment().executable(), QStringLiteral("/usr/bin/python3"));
    QCOMPARE(parsed->environment().versionMajor(), 3);
    QCOMPARE(parsed->environment().versionMinor(), 12);
    QCOMPARE(parsed->environment().cacheTag(), QStringLiteral("cpython-312"));
    QVERIFY(parsed->environment().isIsolated());
    QVERIFY(parsed->package().isFound());
    QVERIFY(parsed->package().isUsable());
    QCOMPARE(parsed->package().version(), QStringLiteral("0.1.0"));
    QCOMPARE(parsed->package().moduleFile(), QStringLiteral("/env/lib/python3.12/site-packages/ovito/__init__.py"));
    QVERIFY(parsed->features() == PythonContract::defaultRequiredFeatures());
    QCOMPARE(parsed->featureNames(), PythonContract::featureNames(PythonContract::defaultRequiredFeatures()));

    // A handshake describes an environment before any compatibility rule is applied: the description alone is valid
    // even when nothing about the environment is supported.
    PythonHandshake old;
    old.setProtocolName(PythonContract::handshakeName())
        .setProtocolVersion(QStringLiteral("1.0"))
        .setEnvironment(environment.setVersion(QStringLiteral("2.7.18")).setVersionInfo(2, 7, 18).setImplementation(QStringLiteral("cpython")));
    QVERIFY(old.isValid());
}

void PythonEnvironmentProbeTest::handshake_keeps_features_of_a_newer_protocol()
{
    // An environment that speaks a newer minor protocol may offer features this build does not know. They are kept
    // verbatim so that a failure can quote them, and only the known names take part in a requirement check.
    const QVariantMap json = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.3") },
        { QStringLiteral("python"),
          QVariantMap({ { QStringLiteral("implementation"), QStringLiteral("cpython") },
                        { QStringLiteral("version"), QStringLiteral("3.13.1") },
                        { QStringLiteral("executable"), QStringLiteral("/usr/bin/python3.13") },
                        { QStringLiteral("platform"), QStringLiteral("linux") } }) },
        { QStringLiteral("package"),
          QVariantMap({ { QStringLiteral("name"), QStringLiteral("ovito") },
                        { QStringLiteral("found"), true },
                        { QStringLiteral("imported"), true },
                        { QStringLiteral("version"), QStringLiteral("0.2.0") },
                        { QStringLiteral("protocolVersion"), QStringLiteral("1.3") },
                        { QStringLiteral("features"), QStringList({ QStringLiteral("function.inplace"),
                                                                   QStringLiteral("schema.introspection"),
                                                                   QStringLiteral("array.buffer"),
                                                                   QStringLiteral("bridge.remote-objects") }) } }) },
        { QStringLiteral("features"), QStringList({ QStringLiteral("function.inplace"),
                                                    QStringLiteral("schema.introspection"),
                                                    QStringLiteral("array.buffer"),
                                                    QStringLiteral("bridge.remote-objects") }) }
    };

    QString error;
    const std::optional<PythonHandshake> handshake = PythonHandshake::fromJson(json, &error);
    QVERIFY2(handshake.has_value(), qPrintable(error));
    QCOMPARE(handshake->protocolVersion(), QStringLiteral("1.3"));
    QCOMPARE(handshake->protocolVersionMinor(), 3);
    QCOMPARE(handshake->features().size(), 3);
    QCOMPARE(handshake->unknownFeatures(), QStringList({ QStringLiteral("bridge.remote-objects") }));
    QCOMPARE(handshake->featureNames(),
             QStringList({ QStringLiteral("function.inplace"), QStringLiteral("schema.introspection"),
                           QStringLiteral("array.buffer"), QStringLiteral("bridge.remote-objects") }));

    // The unknown name travels back out unchanged, so nothing is lost by one build inspecting another's answer.
    const QVariantMap roundTripped = handshake->toJson();
    QCOMPARE(roundTripped.value(QStringLiteral("features")).toStringList(),
             QStringList({ QStringLiteral("function.inplace"), QStringLiteral("schema.introspection"),
                           QStringLiteral("array.buffer"), QStringLiteral("bridge.remote-objects") }));
}

void PythonEnvironmentProbeTest::handshake_rejects_malformed_messages()
{
    QString error;

    // A missing protocol name: the answer is not a handshake at all.
    QVERIFY(!PythonHandshake::fromJson({ { QStringLiteral("protocolVersion"), QStringLiteral("1.0") } }, &error).has_value());
    QVERIFY(!error.isEmpty());

    // A foreign protocol: a valid answer of something else.
    const QVariantMap foreign = { { QStringLiteral("handshake"), QStringLiteral("ovito.other.protocol") },
                                  { QStringLiteral("protocolVersion"), QStringLiteral("1.0") } };
    QVERIFY(!PythonHandshake::fromJson(foreign, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("ovito.other.protocol")));

    // A version that is not a version number.
    const QVariantMap badVersion = { { QStringLiteral("handshake"), PythonContract::handshakeName() },
                                     { QStringLiteral("protocolVersion"), QStringLiteral("one") } };
    QVERIFY(!PythonHandshake::fromJson(badVersion, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("one")));

    // An interpreter description without the fields the validation needs.
    const QVariantMap incomplete = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("python"), QVariantMap({ { QStringLiteral("version"), QStringLiteral("3.12.3") } }) }
    };
    QVERIFY(!PythonHandshake::fromJson(incomplete, &error).has_value());

    // An interpreter version that is not a number, without the structured form as a fallback.
    const QVariantMap badPythonVersion = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("python"),
          QVariantMap({ { QStringLiteral("implementation"), QStringLiteral("cpython") },
                        { QStringLiteral("version"), QStringLiteral("3.x.3") },
                        { QStringLiteral("executable"), QStringLiteral("/usr/bin/python3") },
                        { QStringLiteral("platform"), QStringLiteral("linux") } }) }
    };
    QVERIFY(!PythonHandshake::fromJson(badPythonVersion, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("3.x.3")));
}

/******************************************************************************
* The interpreter of this machine.
******************************************************************************/
void PythonEnvironmentProbeTest::probe_finds_the_interpreter_of_this_machine()
{
    SKIP_WITHOUT_PYTHON();

    PythonProbeRequest request = PythonProbeRequest::forExecutable(interpreter);
    QVERIFY(!request.scriptFile().isEmpty());
    QVERIFY(request.isIsolated());
    QCOMPARE(request.commandLine().mid(1, 1), QStringList({ QStringLiteral("-I") }));

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(request);

    // This machine has no ovito package, so the verdict is PackageMissing - unless someone installed one, in which
    // case the same probe must have accepted it. Both are a statement about the machine, and the parts that are about
    // the interpreter have to hold either way.
    QVERIFY(result.status() == PythonContract::ProbeStatus::PackageMissing ||
            result.status() == PythonContract::ProbeStatus::Compatible);
    QVERIFY(result.handshake().has_value());
    const PythonEnvironmentInfo& environment = result.handshake()->environment();
    QCOMPARE(environment.implementation(), QStringLiteral("cpython"));
    QVERIFY(PythonContract::supportsPythonVersion(environment.versionMajor(), environment.versionMinor()));
    QVERIFY(QFileInfo(environment.executable()).isAbsolute());
    QCOMPARE(QFileInfo(environment.executable()).canonicalFilePath(), QFileInfo(interpreter).canonicalFilePath());
    QVERIFY(!environment.cacheTag().isEmpty());
    QVERIFY(environment.isIsolated());

    // The verdict explains itself, and the details carry the machine-readable form of the same statement.
    if(result.status() == PythonContract::ProbeStatus::PackageMissing) {
        QVERIFY(result.message().contains(QStringLiteral("ovito")));
        QVERIFY(result.message().contains(QStringLiteral("does not install")));
        QVERIFY(!result.details().value(QStringLiteral("supportedPython")).toString().isEmpty());
        QVERIFY(result.details().contains(QStringLiteral("executable")));
        QCOMPARE(result.details().value(QStringLiteral("packageFound")).toBool(), false);
    }
    else {
        QVERIFY(result.handshake()->package().isUsable());
    }
    QCOMPARE(result.statusName(), PythonContract::probeStatusName(result.status()));
    QCOMPARE(result.toJson().value(QStringLiteral("status")).toString(), result.statusName());
    QCOMPARE(result.toJson().value(QStringLiteral("compatible")).toBool(), result.isCompatible());
}

void PythonEnvironmentProbeTest::probe_blocking_matches_the_asynchronous_interface()
{
    SKIP_WITHOUT_PYTHON();

    // Both interfaces report the same environment; the blocking one is a wrapper, not a second implementation.
    PythonProbeResult asynchronous;
    int verdicts = 0;
    PythonEnvironmentProbe probe;
    connect(&probe, &PythonEnvironmentProbe::finished, this, [&](const PythonProbeResult& result) {
        asynchronous = result;
        ++verdicts;
    });
    probe.start(PythonProbeRequest::forExecutable(interpreter));
    QVERIFY(probe.isRunning());
    QTRY_VERIFY_WITH_TIMEOUT(verdicts == 1, 20000);
    QVERIFY(!probe.isRunning());

    const PythonProbeResult blocking = PythonEnvironmentProbe::probeBlocking(PythonProbeRequest::forExecutable(interpreter));
    QCOMPARE(asynchronous.status(), blocking.status());
    QVERIFY(blocking.handshake().has_value());
    QCOMPARE(blocking.handshake()->environment().version(), asynchronous.handshake()->environment().version());
    QCOMPARE(blocking.handshake()->environment().executable(), asynchronous.handshake()->environment().executable());
}

/******************************************************************************
* Fixture packages: one case per compatibility rule.
******************************************************************************/
void PythonEnvironmentProbeTest::probe_accepts_a_compatible_package()
{
    SKIP_WITHOUT_PYTHON();
    const QString directory = writeFixturePackage(PythonContract::featureNames(PythonContract::defaultRequiredFeatures()));
    QVERIFY(!directory.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixturePackage(interpreter, directory));
    QVERIFY2(result.isCompatible(), qPrintable(result.message()));
    QCOMPARE(result.statusName(), QStringLiteral("compatible"));
    QVERIFY(result.handshake().has_value());
    QVERIFY(result.handshake()->package().isUsable());
    QCOMPARE(result.handshake()->package().version(), QStringLiteral("0.1.0"));
    QCOMPARE(result.handshake()->package().protocolVersion(), QStringLiteral("1.0"));
    QVERIFY(result.handshake()->package().moduleFile().contains(QStringLiteral("ovito")));
    QVERIFY(result.handshake()->features() == PythonContract::defaultRequiredFeatures());
    QCOMPARE(result.details().value(QStringLiteral("packageVersion")).toString(), QStringLiteral("0.1.0"));
    QCOMPARE(result.details().value(QStringLiteral("requiredFeatures")).toStringList(),
             PythonContract::featureNames(PythonContract::defaultRequiredFeatures()));
    QCOMPARE(result.details().value(QStringLiteral("advertisedFeatures")).toStringList(),
             PythonContract::featureNames(PythonContract::defaultRequiredFeatures()));
    QVERIFY(result.message().isEmpty());
}

void PythonEnvironmentProbeTest::probe_reports_a_package_that_cannot_be_imported()
{
    SKIP_WITHOUT_PYTHON();
    // The package is present in the environment, and importing it raises: exactly the mixed installation the design
    // says a manifest cannot rule out.
    const QString directory = writeFixturePackage(PythonContract::featureNames(PythonContract::defaultRequiredFeatures()),
                                                 QStringLiteral("1.0"), QStringLiteral("0.1.0"),
                                                 QStringLiteral("raise RuntimeError(\"the fixture refuses to load\")"));
    QVERIFY(!directory.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixturePackage(interpreter, directory));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::PackageIncompatible);
    QVERIFY(result.message().contains(QStringLiteral("could not be imported")));
    QVERIFY(result.message().contains(QStringLiteral("the fixture refuses to load")));
    QVERIFY(result.details().value(QStringLiteral("importError")).toString().contains(QStringLiteral("RuntimeError")));
    QCOMPARE(result.details().value(QStringLiteral("packageFound")).toBool(), true);
    QCOMPARE(result.details().value(QStringLiteral("packageImported")).toBool(), false);
    // The interpreter is still reported: a rejected environment is described, not hidden.
    QVERIFY(result.handshake().has_value());
    QCOMPARE(result.handshake()->environment().implementation(), QStringLiteral("cpython"));
}

void PythonEnvironmentProbeTest::probe_reports_an_incompatible_protocol_version()
{
    SKIP_WITHOUT_PYTHON();
    const QString directory = writeFixturePackage(PythonContract::featureNames(PythonContract::defaultRequiredFeatures()),
                                                 QStringLiteral("2.0"));
    QVERIFY(!directory.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixturePackage(interpreter, directory));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::PackageIncompatible);
    QVERIFY(result.message().contains(QStringLiteral("protocol 2.0")));
    QVERIFY(result.message().contains(PythonContract::protocolVersion()));
    QCOMPARE(result.details().value(QStringLiteral("packageProtocolVersion")).toString(), QStringLiteral("2.0"));
    QCOMPARE(result.details().value(QStringLiteral("expectedProtocolVersion")).toString(), PythonContract::protocolVersion());
}

void PythonEnvironmentProbeTest::probe_reports_a_missing_feature()
{
    SKIP_WITHOUT_PYTHON();
    // A package of the right protocol version that cannot run the first modifier contract: metadata only.
    const QString directory = writeFixturePackage({ QStringLiteral("schema.introspection") });
    QVERIFY(!directory.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixturePackage(interpreter, directory));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::FeatureMissing);
    QCOMPARE(result.details().value(QStringLiteral("missingFeatures")).toStringList(),
             QStringList({ QStringLiteral("function.inplace"), QStringLiteral("array.buffer") }));
    QVERIFY(result.message().contains(QStringLiteral("function.inplace")));
    QVERIFY(result.message().contains(QStringLiteral("0.1.0")));

    // A caller that needs only what the package offers is satisfied by the same environment.
    PythonProbeRequest narrow = requestForFixturePackage(interpreter, directory);
    narrow.setRequiredFeatures({ PythonContract::Feature::SchemaIntrospection });
    QVERIFY(PythonEnvironmentProbe::probeBlocking(narrow).isCompatible());
}

void PythonEnvironmentProbeTest::probe_reports_an_unsupported_python()
{
    SKIP_WITHOUT_PYTHON();

    // A fixture handshake that claims an interpreter this build does not support. The rule is about the interpreter,
    // not about the package, so the package in the fixture is a perfectly good one.
    const QVariantMap python = {
        { QStringLiteral("implementation"), QStringLiteral("cpython") },
        { QStringLiteral("version"), QStringLiteral("3.6.9") },
        { QStringLiteral("versionMajor"), 3 },
        { QStringLiteral("versionMinor"), 6 },
        { QStringLiteral("versionMicro"), 9 },
        { QStringLiteral("executable"), interpreter },
        { QStringLiteral("platform"), QStringLiteral("linux") },
        { QStringLiteral("machine"), QStringLiteral("x86_64") }
    };
    const QVariantMap package = {
        { QStringLiteral("name"), QStringLiteral("ovito") },
        { QStringLiteral("found"), true },
        { QStringLiteral("imported"), true },
        { QStringLiteral("version"), QStringLiteral("0.1.0") },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("features"), PythonContract::featureNames(PythonContract::defaultRequiredFeatures()) }
    };
    const QVariantMap handshake = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("python"), python },
        { QStringLiteral("package"), package },
        { QStringLiteral("features"), package.value(QStringLiteral("features")) }
    };
    const QString script = writeFixtureHandshakeScript(handshake);
    QVERIFY(!script.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, script));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::PythonUnsupported);
    QVERIFY(result.message().contains(QStringLiteral("CPython 3.6.9")));
    QVERIFY(result.message().contains(QStringLiteral("3.10")));
    QCOMPARE(result.details().value(QStringLiteral("detectedPython")).toString(), QStringLiteral("3.6.9"));
    QCOMPARE(result.handshake()->protocolVersion(), QStringLiteral("1.0"));

    // A non-CPython implementation is rejected by the same rule, whatever its version claims.
    QVariantMap pypyPython = python;
    pypyPython.insert(QStringLiteral("implementation"), QStringLiteral("pypy"));
    pypyPython.insert(QStringLiteral("version"), QStringLiteral("3.10.14"));
    QVariantMap pypyHandshake = handshake;
    pypyHandshake.insert(QStringLiteral("python"), pypyPython);
    const QString pypyScript = writeFixtureHandshakeScript(pypyHandshake);
    const PythonProbeResult pypyResult = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, pypyScript));
    QCOMPARE(pypyResult.status(), PythonContract::ProbeStatus::PythonUnsupported);
    // An implementation this build does not support is named as its own modules name it ("pypy"), not as a brand.
    QVERIFY(pypyResult.message().contains(QStringLiteral("pypy 3.10.14")));
}

void PythonEnvironmentProbeTest::probe_reports_an_interpreter_mismatch()
{
    SKIP_WITHOUT_PYTHON();

    // A wrapper or a shim starts some interpreter other than the selected one; `sys.executable` is what reveals it.
    const QVariantMap handshake = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("python"),
          QVariantMap({ { QStringLiteral("implementation"), QStringLiteral("cpython") },
                        { QStringLiteral("version"), QStringLiteral("3.12.3") },
                        { QStringLiteral("executable"), QStringLiteral("/somewhere/else/bin/python3") },
                        { QStringLiteral("prefix"), QStringLiteral("/somewhere/else") },
                        { QStringLiteral("basePrefix"), QStringLiteral("/somewhere/else") },
                        { QStringLiteral("platform"), QStringLiteral("linux") },
                        { QStringLiteral("machine"), QStringLiteral("x86_64") } }) },
        { QStringLiteral("package"),
          QVariantMap({ { QStringLiteral("name"), QStringLiteral("ovito") },
                        { QStringLiteral("found"), true },
                        { QStringLiteral("imported"), true },
                        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
                        { QStringLiteral("features"),
                          PythonContract::featureNames(PythonContract::defaultRequiredFeatures()) } }) }
    };
    const QString script = writeFixtureHandshakeScript(handshake);
    QVERIFY(!script.isEmpty());

    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, script));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::InterpreterMismatch);
    QVERIFY(result.message().contains(QStringLiteral("/somewhere/else/bin/python3")));
    QVERIFY(result.message().contains(interpreter));
    QCOMPARE(result.details().value(QStringLiteral("sysExecutable")).toString(), QStringLiteral("/somewhere/else/bin/python3"));
    QCOMPARE(result.details().value(QStringLiteral("executable")).toString(), interpreter);
}

/******************************************************************************
* Failure modes of the process itself.
******************************************************************************/
void PythonEnvironmentProbeTest::probe_reports_an_interpreter_that_does_not_exist()
{
    // No interpreter, no fixture, no interpreter needed: a path that cannot be started is reported, and quickly.
    QElapsedTimer timer;
    timer.start();
    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(
        PythonProbeRequest::forExecutable(_tempDirectory.filePath(QStringLiteral("no-such-interpreter")), 5000));
    QCOMPARE(result.status(), PythonContract::ProbeStatus::InterpreterMissing);
    QVERIFY(result.message().contains(QStringLiteral("could not be started")));
    QVERIFY(result.details().value(QStringLiteral("error")).toString().isEmpty() == false);
    QVERIFY(timer.elapsed() < 5000);
    QVERIFY(!result.handshake().has_value());
}

void PythonEnvironmentProbeTest::probe_reports_an_interpreter_that_does_not_answer()
{
    SKIP_WITHOUT_PYTHON();

    // An interpreter that runs but prints no handshake at all.
    const QString silent = writeFixture(QStringLiteral("silent.py"), QStringLiteral("import sys\nsys.exit(0)\n"));
    QVERIFY(!silent.isEmpty());
    const PythonProbeResult silentResult = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, silent));
    QCOMPARE(silentResult.status(), PythonContract::ProbeStatus::InterpreterFailed);
    QVERIFY(silentResult.message().contains(QStringLiteral("without a handshake")));
    QCOMPARE(silentResult.details().value(QStringLiteral("executable")).toString(), interpreter);

    // An interpreter that exits with an error: its stderr is part of the report, because that is what the user has to
    // see to repair the environment.
    const QString failing = writeFixture(QStringLiteral("failing.py"),
                                         QStringLiteral("import sys\nsys.stderr.write(\"fixture failure\\n\")\nsys.exit(3)\n"));
    const PythonProbeResult failingResult = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, failing));
    QCOMPARE(failingResult.status(), PythonContract::ProbeStatus::InterpreterFailed);
    QCOMPARE(failingResult.details().value(QStringLiteral("exitCode")).toInt(), 3);
    QCOMPARE(failingResult.details().value(QStringLiteral("stderr")).toString(), QStringLiteral("fixture failure"));
}

void PythonEnvironmentProbeTest::probe_reports_an_answer_that_is_not_a_handshake()
{
    SKIP_WITHOUT_PYTHON();

    // The interpreter is healthy, the answer is not this protocol: that is a protocol error, not a broken environment.
    const QString garbage = writeFixture(QStringLiteral("garbage.py"), QStringLiteral("print(\"this is not JSON\")\n"));
    const PythonProbeResult garbageResult = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, garbage));
    QCOMPARE(garbageResult.status(), PythonContract::ProbeStatus::InterpreterFailed);
    QVERIFY(garbageResult.details().value(QStringLiteral("answer")).toString().contains(QStringLiteral("this is not JSON")));

    // A well-formed JSON answer of another protocol.
    const QString foreign = writeFixture(QStringLiteral("foreign.py"),
                                         QStringLiteral("import json\nprint(json.dumps({\"handshake\": \"other.protocol\"}))\n"));
    const PythonProbeResult foreignResult = PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, foreign));
    QCOMPARE(foreignResult.status(), PythonContract::ProbeStatus::ProtocolError);
    QVERIFY(foreignResult.message().contains(QStringLiteral("other.protocol")));
    QVERIFY(!foreignResult.handshake().has_value());

    // The probe reads the last line of the output, so an interpreter or a site hook that prints a warning first still
    // gets its handshake validated.
    const QVariantMap handshake = {
        { QStringLiteral("handshake"), PythonContract::handshakeName() },
        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
        { QStringLiteral("python"),
          QVariantMap({ { QStringLiteral("implementation"), QStringLiteral("cpython") },
                        { QStringLiteral("version"), QStringLiteral("3.12.3") },
                        { QStringLiteral("executable"), interpreter },
                        { QStringLiteral("platform"), QStringLiteral("linux") },
                        { QStringLiteral("machine"), QStringLiteral("x86_64") } }) },
        { QStringLiteral("package"),
          QVariantMap({ { QStringLiteral("name"), QStringLiteral("ovito") },
                        { QStringLiteral("found"), true },
                        { QStringLiteral("imported"), true },
                        { QStringLiteral("protocolVersion"), QStringLiteral("1.0") },
                        { QStringLiteral("features"),
                          PythonContract::featureNames(PythonContract::defaultRequiredFeatures()) } }) }
    };
    const QString noisy = writeFixtureHandshakeScript(handshake, QStringLiteral("print(\"a warning from a site hook\")\n"));
    QVERIFY(!noisy.isEmpty());
    QVERIFY(PythonEnvironmentProbe::probeBlocking(requestForFixtureScript(interpreter, noisy)).isCompatible());
}

void PythonEnvironmentProbeTest::probe_reports_a_timeout()
{
    SKIP_WITHOUT_PYTHON();

    // An interpreter that blocks forever must be given up on, not waited for.
    const QString sleeping = writeFixture(QStringLiteral("sleeping.py"),
                                          QStringLiteral("import time\ntime.sleep(60)\n"));
    QVERIFY(!sleeping.isEmpty());

    PythonProbeRequest request = requestForFixtureScript(interpreter, sleeping);
    request.setTimeoutMs(400);
    QElapsedTimer timer;
    timer.start();
    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(request);
    const qint64 elapsed = timer.elapsed();

    QCOMPARE(result.status(), PythonContract::ProbeStatus::Timeout);
    QVERIFY(result.message().contains(QStringLiteral("400 ms")));
    QCOMPARE(result.details().value(QStringLiteral("timeoutMs")).toInt(), 400);
    QVERIFY2(elapsed < 10000, qPrintable(QStringLiteral("The probe waited %1 ms for a hanging interpreter.").arg(elapsed)));
}

void PythonEnvironmentProbeTest::probe_reports_a_missing_probe_script()
{
    SKIP_WITHOUT_PYTHON();

    // A request that names a script that does not exist is a configuration error of the caller, and the probe says so
    // instead of starting an interpreter that would fail for a less useful reason.
    PythonProbeRequest request = PythonProbeRequest::forExecutable(interpreter);
    request.setScriptFile(_tempDirectory.filePath(QStringLiteral("no-such-probe.py")));
    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(request);
    QCOMPARE(result.status(), PythonContract::ProbeStatus::NotConfigured);
    QCOMPARE(result.details().value(QStringLiteral("scriptFile")).toString(), request.scriptFile());
    QVERIFY(result.message().contains(QStringLiteral("does not exist")));
}

void PythonEnvironmentProbeTest::probe_reports_a_missing_configuration()
{
    // Nothing configured: no interpreter is nothing to probe, and the message says what to do about it.
    const PythonProbeResult result = PythonEnvironmentProbe::probeBlocking(PythonProbeRequest());
    QCOMPARE(result.status(), PythonContract::ProbeStatus::NotConfigured);
    QVERIFY(result.message().contains(QStringLiteral("Select a Python environment")));
    QVERIFY(!result.handshake().has_value());

    // The interpreter of this machine is a candidate, never a silent fallback: a caller has to pass it explicitly.
    const QString interpreter = PythonEnvironmentProbe::findInterpreter();
    if(!interpreter.isEmpty())
        QVERIFY(QFileInfo(interpreter).isAbsolute());
}

void PythonEnvironmentProbeTest::probe_can_be_cancelled()
{
    SKIP_WITHOUT_PYTHON();
    const QString sleeping = writeFixture(QStringLiteral("cancel-me.py"), QStringLiteral("import time\ntime.sleep(60)\n"));
    QVERIFY(!sleeping.isEmpty());

    PythonProbeResult verdict;
    int verdicts = 0;
    PythonEnvironmentProbe probe;
    connect(&probe, &PythonEnvironmentProbe::finished, this, [&](const PythonProbeResult& result) {
        verdict = result;
        ++verdicts;
    });

    PythonProbeRequest request = requestForFixtureScript(interpreter, sleeping);
    request.setTimeoutMs(60000);
    probe.start(request);
    QVERIFY(probe.isRunning());
    QTest::qWait(50);
    QCOMPARE(verdicts, 0);

    QElapsedTimer timer;
    timer.start();
    probe.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(verdicts == 1, 5000);
    QVERIFY2(timer.elapsed() < 5000, "Cancelling a probe did not stop its interpreter.");
    QCOMPARE(verdict.status(), PythonContract::ProbeStatus::Cancelled);
    QVERIFY(verdict.message().contains(QStringLiteral("cancelled")));
    QVERIFY(!probe.isRunning());

    // Cancelling again is harmless: there is nothing running any more.
    probe.cancel();
    QCOMPARE(verdicts, 1);
}

void PythonEnvironmentProbeTest::probe_emits_exactly_one_verdict()
{
    SKIP_WITHOUT_PYTHON();

    // The asynchronous contract: start() returns before the environment has answered, and the caller gets one verdict.
    int verdicts = 0;
    PythonProbeResult verdict;
    PythonEnvironmentProbe probe;
    connect(&probe, &PythonEnvironmentProbe::finished, this, [&](const PythonProbeResult& result) {
        verdict = result;
        ++verdicts;
    });

    QElapsedTimer timer;
    timer.start();
    probe.start(PythonProbeRequest::forExecutable(interpreter));
    const qint64 startElapsed = timer.elapsed();
    QVERIFY(probe.isRunning());
    QVERIFY2(startElapsed < 1000, qPrintable(QStringLiteral("start() blocked for %1 ms.").arg(startElapsed)));

    QTRY_VERIFY_WITH_TIMEOUT(verdicts == 1, 20000);
    QTest::qWait(50);
    QCOMPARE(verdicts, 1);
    QVERIFY(!probe.isRunning());
    QVERIFY(verdict.handshake().has_value());

    // A probe that is given no interpreter ends synchronously; the verdict still arrives exactly once.
    PythonEnvironmentProbe second;
    int secondVerdicts = 0;
    connect(&second, &PythonEnvironmentProbe::finished, this, [&](const PythonProbeResult&) { ++secondVerdicts; });
    second.start(PythonProbeRequest());
    QCOMPARE(secondVerdicts, 1);
    QVERIFY(!second.isRunning());
}

/******************************************************************************
* Helpers.
******************************************************************************/
QString PythonEnvironmentProbeTest::writeFixture(const QString& relativePath, const QString& content)
{
    const QString path = _tempDirectory.filePath(relativePath);
    if(!QDir().mkpath(QFileInfo(path).absolutePath()))
        return {};
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        return {};
    file.write(content.toUtf8());
    file.close();
    return path;
}

QString PythonEnvironmentProbeTest::writeFixturePackage(const QStringList& features, const QString& protocolVersion,
                                                       const QString& version, const QString& importError)
{
    // A `ovito` package whose handshake describes itself; a later minor protocol version and unknown features are just
    // data in the answer, which is exactly how the real package will look to an older build.
    static int counter = 0;
    const QString directory = QStringLiteral("packages/%1").arg(++counter);
    if(writeFixture(directory + QStringLiteral("/ovito/__init__.py"), QStringLiteral("__version__ = \"%1\"\n").arg(version)).isEmpty())
        return {};
    QString automation = QStringLiteral("def handshake():\n"
                                       "    return {\"name\": \"ovito\",\n"
                                       "            \"version\": \"%1\",\n"
                                       "            \"protocolVersion\": \"%2\",\n"
                                       "            \"features\": %3,\n"
                                       "            \"moduleFile\": __file__}\n")
                             .arg(version, protocolVersion, features.isEmpty() ? QStringLiteral("[]")
                                                                              : QStringLiteral("[\"%1\"]").arg(features.join(QStringLiteral("\", \""))));
    if(!importError.isEmpty())
        automation.prepend(importError + QStringLiteral("\n"));
    if(writeFixture(directory + QStringLiteral("/ovito/automation/__init__.py"), automation).isEmpty())
        return {};
    return _tempDirectory.filePath(directory);
}

QString PythonEnvironmentProbeTest::writeFixtureHandshakeScript(const QVariantMap& handshake, const QString& prologue)
{
    static int counter = 0;
    const QString json =
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(handshake)).toJson(QJsonDocument::Compact));
    return writeFixture(QStringLiteral("scripts/handshake-%1.py").arg(++counter),
                        prologue + QStringLiteral("import json\nprint(json.dumps(json.loads(r'''%1''')))\n").arg(json));
}

PythonProbeRequest PythonEnvironmentProbeTest::requestForFixturePackage(const QString& interpreter, const QString& packageDirectory)
{
    // The fixture package lives in a directory, so the interpreter must not be isolated: `-I` ignores PYTHONPATH.
    PythonProbeRequest request = PythonProbeRequest::forExecutable(interpreter);
    request.setIsolated(false);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONPATH"), packageDirectory);
    request.setEnvironment(environment);
    return request;
}

PythonProbeRequest PythonEnvironmentProbeTest::requestForFixtureScript(const QString& interpreter, const QString& scriptPath)
{
    PythonProbeRequest request = PythonProbeRequest::forExecutable(interpreter);
    request.setScriptFile(scriptPath);
    return request;
}

QTEST_MAIN(PythonEnvironmentProbeTest)
#include "tst_python_environment_probe.moc"
