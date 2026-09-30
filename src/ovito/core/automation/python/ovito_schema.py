# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT
"""Reads the schema of a Python script *without running it*, or imports it when the caller asks for that explicitly.

This script is the counterpart of `PythonIntrospector` in `ovito/core/automation/python`. An AI plan, a user or a
frontend wants to know what a script offers - which function is the modifier, what parameters it takes, what their
defaults are - before anything of it is executed, and the answer must not require the caller to trust the script. So
there are two modes and they are different in kind:

  * `--mode ast` (the default) parses the file with :mod:`ast` and reads the decorators and signatures off the syntax
    tree. **Nothing in the file runs** - no import, no top-level statement, no decorator call, not even a module doc
    access that could execute code. What cannot be read off the syntax tree is *reported as unreadable*, never guessed:
    a decorator built at run time, a default value that is an expression, an annotation that is a call. A wrong schema
    is worse than a missing one, because the caller would validate user input against a signature that does not exist.
  * `--mode import` additionally imports the file as a module and reports what happened - including a mapped traceback
    when it raises. This is the mode a frontend uses after the user has agreed to run the file, and it is why the
    design keeps "preview" and "import" apart: only the second one can execute the user's top-level code.

The script prints exactly one JSON object on one line on stdout, in both modes, including when the file has a syntax
error (which is a *diagnostic about the file*, not a failure of this script). It never installs anything, never writes
anything, and never imports a module other than the ones it needs itself. It is deliberately a single file with no
package layout: the real `ovito` package is Phase 4 work, and this is the code that package will absorb.

Exit codes: 0 when a report was printed. A non-zero exit means this script could not run at all - the file was not
readable, or an internal error - which the C++ side reports as a failure of the introspection, not of the script.
"""

import argparse
import ast
import json
import os
import sys
import traceback

#: The report format this script writes. Bumped when the *shape* changes; additive keys do not need a bump.
SCHEMA_VERSION = 1

#: Decorator names that mark a function as the entry point of a Python modifier. The comparison is on the last dotted
#: component, so both `@python_script` and `@ovito.modifiers.python_script` are recognized. Phase 4 owns the final
#: spelling in the shipped package; Phase 2.6 fixes the *reading* rule.
SCHEMA_DECORATORS = ("python_script", "python_modifier", "modifier")

#: Parameter kinds, in the vocabulary of the Python language rather than of `inspect`, so that the report can be read
#: without knowing how CPython represents a parameter.
POSITIONAL_ONLY = "positional_only"
POSITIONAL_OR_KEYWORD = "positional_or_keyword"
KEYWORD_ONLY = "keyword_only"
VAR_POSITIONAL = "var_positional"
VAR_KEYWORD = "var_keyword"

#: The states a listed function can be in. `plain` is an ordinary function, `supported` is a modifier whose schema this
#: script could read off the syntax tree, and `unsupported` is one whose schema it could not - with a reason.
PLAIN = "plain"
SUPPORTED = "supported"
UNSUPPORTED = "unsupported"

#: Literal nodes a default value may consist of before it counts as statically known.
_LITERAL_NODES = (ast.Constant,)


def _unparse(node):
    """The source text of an expression, or `None` when this interpreter cannot produce it.

    `ast.unparse` exists since Python 3.9 and is what makes an annotation or default readable as text. It is only ever
    applied to *source*, never evaluated, so an annotation like `List[float]` is reported as the string it was written
    as and not resolved to a type object.
    """
    try:
        return ast.unparse(node)
    except BaseException:  # noqa: BLE001 - an unparsable annotation is a missing detail, not a failure
        return None


def _is_static(node):
    """Whether an expression can be reported as a *value* without running anything.

    Constants count, and so do the containers and operators built from constants: `2.5`, `-1`, `(1, 2)`, `{"a": 1}`.
    A name, a call or an attribute does not, because its value would have to be computed - which is exactly what the
    AST mode promises not to do.
    """
    if isinstance(node, _LITERAL_NODES):
        return True
    if isinstance(node, ast.UnaryOp):
        return _is_static(node.operand)
    if isinstance(node, (ast.Tuple, ast.List, ast.Set)):
        return all(_is_static(element) for element in node.elts)
    if isinstance(node, ast.Dict):
        return all(_is_static(key) and _is_static(value) for key, value in zip(node.keys, node.values))
    return False


def _decorator_name(node):
    """The dotted name of a decorator expression, or `None` if it is not a plain name/attribute."""
    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.Attribute):
        parent = _decorator_name(node.value)
        return "{}.{}".format(parent, node.attr) if parent else node.attr
    return None


def _is_schema_decorator(node):
    """Whether a decorator expression *names* one of the schema decorators, ignoring how it is called."""
    target = node.func if isinstance(node, ast.Call) else node
    name = _decorator_name(target)
    if name is None:
        return False
    return name.split(".")[-1] in SCHEMA_DECORATORS


def _read_decorator(node, function, diagnostics):
    """Reads one decorator of a function: its name, its arguments and what could not be read.

    Returns `(name, line, arguments, unreadable, dynamicArguments)`. `arguments` is a list of
    `{"name", "position", "value", "dynamic"}` for a call like `@python_script(cutoff=2.5)`; `unreadable` is a
    human-readable reason when the decorator's *identity* is computed at run time, and `dynamicArguments` says that a
    configuration value is - both cases the design says must be reported instead of guessed.
    """
    unreadable = None
    if not isinstance(node, ast.Call):
        name = _decorator_name(node)
        if name is None:
            unreadable = "the decorator is an expression that is evaluated at run time"
        return name, getattr(node, "lineno", None), [], unreadable, False

    target = _decorator_name(node.func)
    if target is None:
        unreadable = "the decorator is called on an expression that is only known at run time"

    arguments = []
    for index, argument in enumerate(node.args):
        value, dynamic = _argument_value(argument)
        arguments.append({"name": None, "position": index, "value": value, "dynamic": dynamic})
    for keyword in node.keywords:
        value, dynamic = _argument_value(keyword.value)
        arguments.append({"name": keyword.arg, "position": None, "value": value, "dynamic": dynamic})

    dynamicArguments = any(argument["dynamic"] for argument in arguments)
    if dynamicArguments:
        # The decorator itself is fine, but what it was configured with is not knowable without running it. The caller
        # gets the decorator name and the arguments it *could* read, plus this diagnostic.
        diagnostics.append({
            "severity": "warning",
            "code": "dynamic_decorator_argument",
            "message": "The decorator \"{}\" of function \"{}\" is configured with an argument that is computed at run "
                       "time, so its value cannot be read without executing the script.".format(
                           target, getattr(function, "name", "?")),
            "line": getattr(node, "lineno", None),
        })
    return target, getattr(node, "lineno", None), arguments, unreadable, dynamicArguments


def _argument_value(node):
    """The reported value of one decorator argument, and whether it had to be left unread."""
    if _is_static(node):
        text = _unparse(node)
        if text is not None:
            try:
                return ast.literal_eval(node), False
            except BaseException:  # noqa: BLE001 - a static-looking expression that will not evaluate is left as text
                return text, False
    text = _unparse(node)
    # An expression whose value is not a literal: report the source, and mark it as not read. The caller must not treat
    # this text as a value.
    return text, True


def _read_parameters(function):
    """Reads the signature of a function off the syntax tree, without calling it."""
    parameters = []
    arguments = function.args

    positional = list(getattr(arguments, "posonlyargs", [])) + list(arguments.args)
    defaults = list(arguments.defaults)
    # `defaults` applies to the *last* n positional parameters.
    defaulted = len(defaults)
    for index, argument in enumerate(positional):
        default_index = index - (len(positional) - defaulted)
        default = defaults[default_index] if default_index >= 0 else None
        parameters.append(_parameter(
            argument,
            POSITIONAL_ONLY if index < len(getattr(arguments, "posonlyargs", [])) else POSITIONAL_OR_KEYWORD,
            default))

    if arguments.vararg is not None:
        parameters.append(_parameter(arguments.vararg, VAR_POSITIONAL, None))
    for argument, default in zip(arguments.kwonlyargs, arguments.kw_defaults):
        parameters.append(_parameter(argument, KEYWORD_ONLY, default))
    if arguments.kwarg is not None:
        parameters.append(_parameter(arguments.kwarg, VAR_KEYWORD, None))
    return parameters


def _parameter(argument, kind, default):
    """One parameter of a signature."""
    annotation = argument.annotation
    entry = {
        "name": argument.arg,
        "kind": kind,
        # An annotation is reported as it was *written* and is never resolved to a type object: that would mean
        # executing the module, which the AST mode promises not to do. Phase 4 owns the typing model that turns these
        # names into something a frontend can render.
        "annotation": _unparse(annotation) if annotation is not None else None,
        "default": None,
        "defaultDynamic": False,
        # A parameter without a default is required: that is the rule the operation schema of the automation contract
        # uses as well, and the reason it is stated here rather than left to the caller.
        "required": default is None,
    }
    if default is not None:
        value, dynamic = _argument_value(default)
        entry["default"] = value if isinstance(value, (bool, int, float, str, list, tuple, dict, type(None))) else str(value)
        entry["defaultDynamic"] = dynamic
        if dynamic:
            entry["required"] = True
    return entry


def _read_function(function, diagnostics):
    """Reads one function definition: its decorators, its signature and whether its schema is readable."""
    entry = {
        "name": function.name,
        "line": function.lineno,
        "docstring": ast.get_docstring(function) or None,
        "decorators": [],
        "decoratorLine": None,
        "isModifier": False,
        "status": PLAIN,
        "reason": None,
        "parameters": _read_parameters(function),
    }

    unreadableDecorator = None
    for decorator in function.decorator_list:
        name, line, arguments, unreadable, dynamicArguments = _read_decorator(decorator, function, diagnostics)
        if unreadable is not None and unreadableDecorator is None:
            unreadableDecorator = unreadable
        entry["decorators"].append({
            "name": name,
            "argumentText": _unparse(decorator) if not isinstance(decorator, ast.Call) else None,
            "arguments": arguments,
            "readable": unreadable is None,
        })
        if _is_schema_decorator(decorator):
            entry["isModifier"] = True
            entry["decoratorLine"] = line
            if unreadable is not None:
                entry["status"] = UNSUPPORTED
                entry["reason"] = unreadable
            elif dynamicArguments:
                # The decorator is recognized, but how it was configured is not knowable. Treating the schema as known
                # would be a guess about the modifier's declared behaviour, so the function is reported as unreadable
                # and the caller has to ask for the import mode (or work with what the report does state).
                entry["status"] = UNSUPPORTED
                entry["reason"] = ("the schema decorator is configured with an argument that is computed at run time, so "
                                   "the declared schema cannot be read without executing the script")

    if not entry["isModifier"] and unreadableDecorator is not None:
        # A decorator that is computed at run time could be the schema decorator or something else entirely, so the
        # function is reported as not readable rather than as an ordinary helper: the caller would otherwise treat a
        # modifier as a plain function and never offer it.
        entry["status"] = UNSUPPORTED
        entry["reason"] = unreadableDecorator

    if entry["isModifier"] and entry["status"] != UNSUPPORTED:
        entry["status"] = SUPPORTED
        if any(parameter["defaultDynamic"] for parameter in entry["parameters"]):
            # A default that has to be computed cannot be reported as a value, so the schema is only partly readable.
            entry["status"] = UNSUPPORTED
            entry["reason"] = ("a parameter default is computed at run time, so the signature cannot be read without "
                               "executing the script")
    return entry


def _read_imports(tree):
    """The modules the script names in its import statements. Nothing is imported; only the names are reported."""
    imports = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imports.append({"module": None, "names": sorted(alias.name for alias in node.names), "line": node.lineno})
        elif isinstance(node, ast.ImportFrom):
            imports.append({"module": node.module, "level": node.level, "line": node.lineno,
                            "names": sorted(alias.name for alias in node.names)})
    return sorted(imports, key=lambda entry: entry["line"])


def _read_with_ast(path, source):
    """The AST mode: everything the syntax tree can say, and a diagnostic for everything it cannot."""
    diagnostics = []
    report = {
        "ok": True,
        "schemaVersion": SCHEMA_VERSION,
        "mode": "ast",
        "path": path,
        "moduleDocstring": None,
        "imports": [],
        "functions": [],
        "diagnostics": diagnostics,
    }
    try:
        tree = ast.parse(source, filename=path)
    except SyntaxError as error:
        # A syntax error is the one case where there is nothing to report but the problem - and it must carry the
        # position, because that is what a frontend shows and what an AI plan has to correct.
        diagnostics.append({
            "severity": "error",
            "code": "syntax_error",
            "message": error.msg or "the file is not valid Python",
            "line": error.lineno,
            "column": error.offset,
            "endLine": getattr(error, "end_lineno", None),
            "endColumn": getattr(error, "end_offset", None),
            "text": (error.text or "").rstrip("\n") or None,
        })
        report["ok"] = False
        return report

    report["moduleDocstring"] = ast.get_docstring(tree) or None
    report["imports"] = _read_imports(tree)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            report["functions"].append(_read_function(node, diagnostics))
    # Only the top level is inspected: a function defined inside another one is not an entry point a caller can select,
    # and reporting it would invite a client to call something the script never meant to expose.
    return report


def _import_module(path):
    """The explicit runtime import: loads the file as a module and reports what happened."""
    import importlib.util

    result = {"attempted": True, "loaded": False, "error": None}
    name = "ovito_user_script_{}".format(abs(hash(os.path.abspath(path))) % (10 ** 12))
    try:
        spec = importlib.util.spec_from_file_location(name, path)
        if spec is None or spec.loader is None:
            result["error"] = {"code": "not_importable", "type": "ImportError",
                               "message": "the file cannot be loaded as a Python module", "traceback": []}
            return result
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        result["loaded"] = True
        result["moduleFile"] = getattr(module, "__file__", None)
        # What the module *reports* about itself is only available when it loaded, which is the difference between the
        # two modes in one place.
        result["hasHandshakeFunction"] = callable(getattr(module, "handshake", None))
    except BaseException as error:  # noqa: BLE001 - user code may raise anything, including SystemExit
        result["error"] = {
            "code": "import_failed",
            "type": type(error).__name__,
            "message": str(error),
            # The mapping the design requires: file, function and line for every frame, innermost last, so a frontend
            # or an AI plan can point at the line that failed instead of showing a raw traceback.
            "traceback": [
                {
                    "file": frame.filename,
                    "line": frame.lineno,
                    "function": frame.name,
                    "text": (frame.line or "").strip() or None,
                    "fromUserScript": os.path.abspath(frame.filename) == os.path.abspath(path),
                }
                for frame in traceback.extract_tb(error.__traceback__)
            ],
        }
    return result


def read_schema(path, mode="ast"):
    """The whole report for one script: the syntax-tree schema, plus the import result in `import` mode."""
    try:
        with open(path, "r", encoding="utf-8") as handle:
            source = handle.read()
    except OSError as error:
        return {
            "ok": False,
            "schemaVersion": SCHEMA_VERSION,
            "mode": mode,
            "path": path,
            "functions": [],
            "imports": [],
            "diagnostics": [{
                "severity": "error",
                "code": "unreadable_file",
                "message": "the file could not be read: {}".format(error.strerror or error),
                "line": None,
                "column": None,
                "text": None,
            }],
        }

    report = _read_with_ast(path, source)
    # The report names the mode that was *requested*, not the mode the syntax tree could read: a caller has to be able
    # to tell whether the import section is missing because nothing was imported or because the import failed.
    report["mode"] = mode
    if mode == "import":
        report["import"] = _import_module(path)
        error = report["import"]["error"]
        if error is not None:
            # The import failure is also a *diagnostic*, so that "the schema could not be read" has one reason in one
            # place whatever the reason was: a syntax error, an unreadable file, or code that raised while loading.
            userFrames = [frame for frame in error["traceback"] if frame["fromUserScript"]]
            report["ok"] = False
            report["diagnostics"].append({
                "severity": "error",
                "code": "import_failed",
                "message": "Importing the file failed: {}: {}".format(error["type"], error["message"]),
                "line": userFrames[-1]["line"] if userFrames else None,
                "column": None,
                "text": userFrames[-1]["text"] if userFrames else None,
            })
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description="Reads the schema of a Python script without running it.")
    parser.add_argument("--path", required=True, help="the script to read")
    parser.add_argument("--mode", default="ast", choices=("ast", "import"),
                        help="ast (default) reads the syntax tree only; import also loads the module")
    parser.add_argument("--pretty", action="store_true", help="indent the JSON report (for humans, not for the caller)")
    arguments = parser.parse_args(argv)

    try:
        report = read_schema(arguments.path, arguments.mode)
    except BaseException as error:  # noqa: BLE001 - the caller must get a report, not a traceback on stderr
        report = {
            "ok": False,
            "schemaVersion": SCHEMA_VERSION,
            "mode": arguments.mode,
            "path": arguments.path,
            "functions": [],
            "imports": [],
            "diagnostics": [{
                "severity": "error",
                "code": "internal_error",
                "message": "{}: {}".format(type(error).__name__, error),
                "line": None,
                "column": None,
                "text": None,
            }],
        }

    if arguments.pretty:
        print(json.dumps(report, indent=2, sort_keys=False))
    else:
        print(json.dumps(report, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
