# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT
"""Describes a Python interpreter to OVITO and checks it for a compatible ovito package.

This script is the counterpart of `PythonEnvironmentProbe` in `ovito/core/automation/python`. It is run *by* a
candidate interpreter, one process per environment, and it prints exactly one JSON object on one line on stdout. The
C++ side owns the decision (see `PythonEnvironmentProbe::validate`); this script only reports facts:

  * facts about the interpreter itself, including `sys.executable`, which is what the application compares with the
    executable it started, and the interpreter's own notion of its platform and architecture;
  * whether an `ovito` package is *present* (an `importlib` metadata lookup, which is an admission filter only);
  * whether that package can actually be *imported* and what it reports about itself through
    `ovito.automation.handshake()` - the authoritative compatibility check.

Phase 2.6 owns this script; it is deliberately a single file with no package layout, because the real package (whose
module this function will live in) is Phase 4 work. It must run on a plain interpreter without any third-party
dependency, and it must not modify the environment, import user code, or write anything anywhere.

Exit codes: 0 when a handshake was printed. A non-zero exit means the interpreter could not run this script at all,
which the C++ side reports as a failure of the environment rather than as an incompatible one.
"""

import json
import platform
import sys

HANDSHAKE_NAME = "ovito.automation.handshake"
HANDSHAKE_PROTOCOL_VERSION = "1.0"


def _interpreter_info():
    """Everything the application is allowed to know about this interpreter."""
    implementation = getattr(sys, "implementation", None)
    return {
        "implementation": implementation.name if implementation is not None else "unknown",
        "version": "{}.{}.{}".format(*sys.version_info[:3]),
        "versionMajor": sys.version_info[0],
        "versionMinor": sys.version_info[1],
        "versionMicro": sys.version_info[2],
        "releaseLevel": sys.version_info[3],
        "executable": sys.executable,
        "prefix": sys.prefix,
        "basePrefix": getattr(sys, "base_prefix", sys.prefix),
        "platform": sys.platform,
        "machine": platform.machine(),
        "architecture": platform.architecture()[0],
        "cacheTag": getattr(implementation, "cache_tag", None) if implementation is not None else None,
        # A free-threaded build has a different ABI and no GIL; report it instead of guessing from the version number.
        "freeThreaded": bool(getattr(sys, "_is_gil_enabled", lambda: True)() is False),
        # Whether this interpreter was started in isolated mode (`-I`: no user site, environment variables ignored).
        "isolated": bool(sys.flags.ignore_environment and sys.flags.no_user_site),
        "maxsize": sys.maxsize,
    }


def _package_info():
    """Presence, importability and self-description of the `ovito` package.

    `found` comes from a metadata lookup that does not execute the package; `imported` is what the application trusts.
    A package that is found but cannot be imported (or that is imported but does not offer the handshake) is reported
    with the error text instead of being silently treated as absent.
    """
    package = {
        "name": "ovito",
        "found": False,
        "imported": False,
        "moduleFile": None,
        "version": None,
        "protocolVersion": None,
        "features": [],
        "bridge": None,
        "error": None,
    }

    try:
        import importlib.util

        spec = importlib.util.find_spec("ovito")
    except BaseException as exc:  # noqa: BLE001 - a broken parent package must be reported, not raised
        package["error"] = "{}: {}".format(type(exc).__name__, exc)
        return package

    if spec is None:
        return package

    package["found"] = True
    package["moduleFile"] = spec.origin

    try:
        from ovito.automation import handshake as package_handshake

        info = package_handshake()
        package["imported"] = True
        package["version"] = info.get("version")
        package["protocolVersion"] = info.get("protocolVersion")
        package["features"] = list(info.get("features", []))
        package["bridge"] = info.get("bridge")
        package["moduleFile"] = info.get("moduleFile", package["moduleFile"])
    except BaseException as exc:  # noqa: BLE001 - an ImportError and an exception raised by the package look alike here
        package["error"] = "{}: {}".format(type(exc).__name__, exc)

    return package


def main():
    package = _package_info()
    handshake = {
        "handshake": HANDSHAKE_NAME,
        "protocolVersion": HANDSHAKE_PROTOCOL_VERSION,
        "python": _interpreter_info(),
        "package": package,
        # The features the application may use in this environment. Without a package there are none: the decorator,
        # the data bridge and the cancellation probe all come from it.
        "features": package["features"] if package["imported"] else [],
    }
    sys.stdout.write(json.dumps(handshake, sort_keys=True) + "\n")
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
