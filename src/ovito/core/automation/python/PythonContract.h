// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>

#include <QStringList>
#include <QVector>

namespace Ovito {

/**
 * \brief The contract between OVITO and a Python environment it runs user code in.
 *
 * OVITO does not ship a Python interpreter. It runs user Python in an environment the user selected, together with an
 * `ovito` package that this repository supplies and that is coupled to the application's protocol version. Two
 * artefacts describe that relationship, and this class is the vocabulary of both:
 *
 *  - a `pyproject.toml` of the supplied package (`requires-python`, dependency bounds, platform markers) that prevents
 *    an installer from *offering* an unsuitable environment - this is an admission filter, not proof;
 *  - a runtime handshake (see PythonHandshake) that the application performs with the interpreter it actually started.
 *    The handshake is the authoritative check, because only it can see whether an already existing environment has been
 *    mixed, which ABI an extension was built for, and what the imported package really reports about itself.
 *
 * Three rules follow from the design and are enforced by this vocabulary rather than by convention:
 *
 *  - **No silent fallback.** The application never substitutes another interpreter and never installs or upgrades a
 *    package. An unsuitable environment is reported with what is missing and how the user can repair it; the caller
 *    decides what to do about it.
 *  - **Features, not version numbers, decide compatibility.** Within one major protocol version the handshake compares
 *    the *features* the environment advertises with the features the application needs. That mirrors the additive-only
 *    minor-version rule of AutomationContract: a newer environment is acceptable when it offers what is needed, and a
 *    version number alone is never treated as proof of anything.
 *  - **The interpreter is identified by what runs, not by what was asked for.** The handshake reports
 *    `sys.executable`; a mismatch with the requested executable (a wrapper script, a shim, a system-wide redirect) is a
 *    reported error rather than a detail, because user Python would otherwise run somewhere the application did not
 *    choose.
 *
 * Like AutomationContract, version 0.x means the contract is still being completed by the Phase 2.6 deliverables.
 */
class OVITO_CORE_EXPORT PythonContract
{
public:

    /**
     * \brief A capability an environment advertises and the application can require.
     *
     * The names are part of the contract and are exchanged as strings in the handshake ("features"). Phase 2.6 fixes
     * the vocabulary; which of the features the supplied package actually implements is decided by Phase 4, which is
     * why the application requires a small, named subset instead of assuming the whole list.
     */
    enum class Feature {

        /// Extract decorator metadata from a source file without executing it (the AST preview of the design).
        SchemaIntrospection,

        /// Run a decorated function with writable input data and a `None` return value: the first modifier contract.
        FunctionInplace,

        /// Declare and transmit scalar (bool/int/float/str) function parameters.
        ParameterScalar,

        /// Transfer numeric arrays through the buffer protocol (memoryview), without a JSON or pickle payload.
        ArrayBuffer,

        /// Transfer numeric arrays through a shared-memory segment both sides map.
        SharedMemoryArray,

        /// Cooperatively observe a cancellation request while a function runs.
        TaskCancellation,

        /// Report a Python exception as structured file/function/line information.
        TracebackMapping
    };

    /// The wire name of a feature, e.g. "array.buffer". Part of the handshake.
    static QString featureName(Feature feature);

    /// Parses a wire name; nullopt for a name this build does not know, which the caller reports as a missing feature.
    static std::optional<Feature> featureFromName(const QString& name);

    /// Every feature this build knows, in declaration order. Used by the tests and by callers that want to explain the
    /// vocabulary; not by any compatibility decision, which only ever looks at the features that were required.
    static QVector<Feature> allFeatures();

    /**
     * \brief The outcome of probing and validating one Python environment.
     *
     * The order of the enumerators is the order in which the probe's validation checks them: a problem of the process
     * itself (was it started, did it answer) is reported before a problem of the interpreter (which Python is it),
     * which is reported before a problem of the package (is it there, may it be used). A caller that gets a status
     * therefore knows that everything before it in this list held.
     */
    enum class ProbeStatus {

        /// The environment is usable: the interpreter is the requested one, it is a supported CPython on a supported
        /// platform, and the package is importable and offers the required features.
        Compatible,

        /// The probe was not configured: no interpreter executable or no probe script was given to it.
        NotConfigured,

        /// The interpreter executable does not exist or could not be started at all.
        InterpreterMissing,

        /// The interpreter was started but did not answer with a usable handshake: it crashed, it printed something
        /// else, or the handshake itself was malformed.
        InterpreterFailed,

        /// The interpreter did not answer within the timeout. The timeout is a property of the probe, not of the
        /// environment: a slow environment is reported as a timeout rather than as a failure.
        Timeout,

        /// The interpreter answered, but not with a well-formed handshake of this protocol: a missing field, an
        /// unparseable version, or another protocol name.
        ProtocolError,

        /// The interpreter is not a supported Python (another implementation, or a version outside the supported
        /// range).
        PythonUnsupported,

        /// A supported Python on a platform or architecture this build does not ship a package for.
        PlatformUnsupported,

        /// The `ovito` package is not installed in this environment.
        PackageMissing,

        /// The `ovito` package is present but not usable: it cannot be imported, or its protocol version is
        /// incompatible with this application.
        PackageIncompatible,

        /// The package is usable but does not offer every feature this caller requires.
        FeatureMissing,

        /// The interpreter that answered is not the interpreter that was started (`sys.executable` differs from the
        /// requested executable).
        InterpreterMismatch,

        /// The probe was cancelled before the environment answered. A cancelled probe is not a verdict about the
        /// environment, so it is the caller's own outcome rather than one of the checks above.
        Cancelled
    };

    /// The wire name of a probe status, e.g. "package_missing". Part of the probe's JSON result.
    static QString probeStatusName(ProbeStatus status);

    /// Parses a wire name; nullopt for an unknown name.
    static std::optional<ProbeStatus> probeStatusFromName(const QString& name);

    /// The name of the handshake message itself ("ovito.automation.handshake"). A response that carries another name
    /// belongs to a different protocol and is rejected as such.
    static QString handshakeName();

    /// The package the application looks for in an environment.
    static QString packageName();

    /// The protocol version of this build, "major.minor". Compared with the package's version as described in the
    /// class comment: the major number must match, the minor number must not be lower than the required features
    /// demand, and the features are what actually decide.
    static QString protocolVersion();
    static constexpr int protocolVersionMajor = 1;
    static constexpr int protocolVersionMinor = 0;

    /// The supported CPython range, inclusive. Outside it the environment is reported as PythonUnsupported regardless
    /// of the package, because a native bridge cannot be promised for it.
    static QVector<int> minimumPythonVersion();
    static QVector<int> maximumPythonVersion();
    static bool supportsPythonVersion(int major, int minor);

    /// The `sys.platform` values this build ships a package for, and the machine architectures it builds for.
    static QStringList supportedPlatforms();
    static QStringList supportedArchitectures();

    /**
     * \brief The features the application requires from an environment by default.
     *
     * The first modifier contract needs metadata extraction and the in-place function call; the array buffer is how its
     * data arrives. A caller that needs more (or, in a test, less) sets the requirement explicitly.
     */
    static QVector<Feature> defaultRequiredFeatures();

    /// Renders a feature list as wire names, for the `details` of a failed probe.
    static QStringList featureNames(const QVector<Feature>& features);

    /// The human-readable core version, e.g. "3.12" for the envelope; used in the probe's error messages.
    static QString versionString(const QVector<int>& version);
};

}   // namespace Ovito
