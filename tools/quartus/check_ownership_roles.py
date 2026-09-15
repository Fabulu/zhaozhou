#!/usr/bin/env python3
"""Require exactly one elaborated provider for each declared ownership role.

Provider identity is semantic metadata, not a naming convention. The manifest is
an explicit registry, and test-only/ future providers may declare the same role
with ``(* zhao_ownership_role = "ROLE" *)`` immediately before their module.
Module names, substrings and graph shape never infer ownership.

``module_graph`` is used only to assemble a conservative source set. Acceptance
comes from Verilator's post-preprocessing, post-generate V3Param AST: dead
generate/ifdef branches and string-shaped text do not produce AST cells, while
macro-expanded and renamed instances do. The current TEXJOIN ruling is scoped
to the selected V3 subsystem, not to ``zhao_shell_top``.
"""

from __future__ import print_function

import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from module_graph import build, load, strip_comments  # noqa: E402

DEFAULT_MANIFEST = "design/prod_manifest.yml"
DEFAULT_RTL = "fpga/rtl"
VALID_SCOPES = {"connected", "selected_subsystem"}
PROVIDER_IDENTIFICATION = "explicit_registry"

# Deliberately duplicated outside the manifest. Losing or renaming one of the
# three known implementations is therefore a checker failure, not a quieter
# ownership result. Adding a real fourth implementation requires an explicit
# registry/checker change and a review of the exactly-one rule.
PINNED_PROVIDER_REGISTRY = {
    "raster_texture_fragment_lifecycle": frozenset({
        "zhao_texture_v3own",
        "zhao_raster_texjoin_v2",
        "zhao_texture_fragrob",
    }),
}

ROLE_ATTRIBUTE_RE = re.compile(
    r"^[ \t]*\(\*[ \t]*zhao_ownership_role[ \t]*=[ \t]*"
    r"\"([A-Za-z_]\w*)\"[ \t]*\*\)[ \t]*(?:\r?\n[ \t]*)?"
    r"module[ \t]+([A-Za-z_]\w*)",
    re.M,
)
AST_CELL_RE = re.compile(
    r"De-parameterize:\s+CELL\b.*?\s([A-Za-z_$][\w$]*)\s+->\s+MODULE\b"
    r".*?\s([A-Za-z_$][\w$]*)\s+L\d+\s+D\d+"
)
PACKAGE_DECL_RE = re.compile(
    r"^[ \t]*package[ \t]+([A-Za-z_]\w*)[ \t]*;", re.M
)
PACKAGE_REF_RE = re.compile(r"\b([A-Za-z_]\w*)::")


class RoleManifestError(ValueError):
    pass


class ElaborationError(RuntimeError):
    pass


def read_roles(path=DEFAULT_MANIFEST):
    """Parse and validate the deliberately small ownership-role subsection."""
    roles = {}
    in_roles = False
    role = None
    in_providers = False

    with io.open(path, encoding="utf-8") as stream:
        lines = list(stream)
    for lineno, raw in enumerate(lines, 1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if re.match(r"^\S", line):
            in_roles = line == "ownership_roles:"
            role = None
            in_providers = False
            continue
        if not in_roles:
            continue

        match = re.match(r"^  ([A-Za-z_]\w*):\s*$", line)
        if match:
            role = match.group(1)
            if role in roles:
                raise RoleManifestError("line %d: duplicate ownership role %s" %
                                        (lineno, role))
            roles[role] = {
                "scope": None,
                "root": None,
                "provider_identification": None,
                "providers": [],
            }
            in_providers = False
            continue
        if role is None:
            raise RoleManifestError("line %d: ownership field has no role" % lineno)

        match = re.match(
            r"^    (scope|root|provider_identification):\s*(\S+)\s*$", line)
        if match:
            key, value = match.groups()
            if roles[role][key] is not None:
                raise RoleManifestError("line %d: duplicate %s for role %s" %
                                        (lineno, key, role))
            roles[role][key] = value
            in_providers = False
            continue
        if line == "    providers:":
            in_providers = True
            continue
        match = re.match(r"^      -\s*(\S+)\s*$", line)
        if match and in_providers:
            provider = match.group(1)
            if provider in roles[role]["providers"]:
                raise RoleManifestError("line %d: duplicate provider %s for role %s" %
                                        (lineno, provider, role))
            roles[role]["providers"].append(provider)
            continue
        raise RoleManifestError("line %d: malformed ownership role row: %s" %
                                (lineno, line.strip()))

    if not roles:
        raise RoleManifestError("manifest has no ownership_roles declarations")
    for name, declaration in roles.items():
        if declaration["scope"] not in VALID_SCOPES:
            raise RoleManifestError("role %s has invalid or missing scope %r" %
                                    (name, declaration["scope"]))
        if not declaration["root"]:
            raise RoleManifestError("role %s has no root" % name)
        if declaration["provider_identification"] != PROVIDER_IDENTIFICATION:
            raise RoleManifestError(
                "role %s must say provider_identification: %s; module names and "
                "graph shape do not infer semantic ownership" %
                (name, PROVIDER_IDENTIFICATION))
        if not declaration["providers"]:
            raise RoleManifestError("role %s has no providers" % name)
        pinned = PINNED_PROVIDER_REGISTRY.get(name)
        if pinned is None:
            raise RoleManifestError(
                "role %s has no checker-pinned provider registry" % name)
        actual = set(declaration["providers"])
        if actual != pinned:
            raise RoleManifestError(
                "role %s provider registry must be exactly [%s], found [%s]" %
                (name, ", ".join(sorted(pinned)), ", ".join(sorted(actual))))
    missing_roles = sorted(set(PINNED_PROVIDER_REGISTRY) - set(roles))
    if missing_roles:
        raise RoleManifestError("manifest is missing checker-pinned role(s): %s" %
                                ", ".join(missing_roles))
    return roles


def _instantiated_modules(text, module_names, own_modules):
    """Conservative reconnaissance only; never used as ownership evidence."""
    body = strip_comments(text)
    found = set()
    for module in module_names:
        if module in own_modules:
            continue
        pattern = (r"(?<![\w.])" + re.escape(module) + r"(?![\w$])"
                   r"\s*(?:#\s*\((?:[^;]*?)\)\s*)?[A-Za-z_]\w*\s*\(")
        if re.search(pattern, body):
            found.add(module)
    return found


def ordered_package_sources(rtl_dir=DEFAULT_RTL):
    """Return every RTL package source in dependency-before-user order."""
    package_source = {}
    package_refs = {}
    for source, text in load(rtl_dir).items():
        body = strip_comments(text)
        declarations = PACKAGE_DECL_RE.findall(body)
        for package in declarations:
            previous = package_source.get(package)
            if previous is not None and previous != source:
                raise RoleManifestError(
                    "package %s is declared by both %s and %s" %
                    (package, previous, source))
            package_source[package] = source
            package_refs[package] = set(PACKAGE_REF_RE.findall(body)) - {package}

    state = {}
    ordered = []
    emitted_sources = set()

    def visit(package, stack):
        status = state.get(package)
        if status == "done":
            return
        if status == "visiting":
            raise RoleManifestError(
                "package dependency cycle: %s" % " -> ".join(stack + [package]))
        state[package] = "visiting"
        for dependency in sorted(package_refs.get(package, ())):
            if dependency in package_source:
                visit(dependency, stack + [package])
        source = package_source[package]
        if source not in emitted_sources:
            ordered.append(os.path.abspath(source))
            emitted_sources.add(source)
        state[package] = "done"

    for package in sorted(package_source):
        visit(package, [])
    return ordered


def module_edges(rtl_dir=DEFAULT_RTL, extra_sources=()):
    """Build a conservative source-discovery graph, including test sources."""
    decl, inst_by_file = build(rtl_dir)
    extra_text = {}
    for source in extra_sources:
        source = os.path.abspath(str(source))
        normalized = source.replace(os.sep, "/")
        with io.open(source, encoding="utf-8", errors="replace") as stream:
            text = stream.read()
        extra_text[normalized] = text
        for match in re.finditer(r"^\s*module\s+([A-Za-z_]\w*)", text, re.M):
            module = match.group(1)
            if module in decl:
                raise RoleManifestError("extra source redeclares module %s" % module)
            decl[module] = normalized

    modules_by_file = {}
    for module, source in decl.items():
        modules_by_file.setdefault(source, []).append(module)

    edges = {}
    for source, children in inst_by_file.items():
        for owner in modules_by_file.get(source, ()):
            edges.setdefault(owner, set()).update(children)

    names = set(decl)
    for source, text in extra_text.items():
        owners = set(modules_by_file[source])
        children = _instantiated_modules(text, names, owners)
        for owner in owners:
            edges.setdefault(owner, set()).update(children)
    return decl, edges, extra_text


def closure(edges, root):
    seen = set()
    stack = [root]
    while stack:
        current = stack.pop()
        for child in edges.get(current, ()):
            if child not in seen:
                seen.add(child)
                stack.append(child)
    return seen


def explicit_source_annotations(decl, extra_text):
    """Return role -> explicitly annotated module names.

    Production providers are explicitly registered in the manifest. This source
    attribute is the independent extension point used by test mutants and by any
    future implementation before it may enter the registry.
    """
    text_by_source = dict(extra_text)
    for source in set(decl.values()):
        if source in text_by_source:
            continue
        try:
            with io.open(source, encoding="utf-8", errors="replace") as stream:
                text_by_source[source] = stream.read()
        except OSError:
            continue

    annotated = {}
    for text in text_by_source.values():
        body = strip_comments(text)
        for role, module in ROLE_ATTRIBUTE_RE.findall(body):
            annotated.setdefault(role, set()).add(module)
    return annotated


def _repo_root(manifest):
    path = os.path.abspath(str(manifest))
    return os.path.dirname(os.path.dirname(path))


def find_verilator(repo_root):
    explicit = os.environ.get("ZHAO_VERILATOR")
    if explicit and os.path.isfile(explicit):
        return explicit

    vroot = os.environ.get("VERILATOR_ROOT")
    candidates = []
    if vroot:
        candidates.extend([
            os.path.normpath(os.path.join(vroot, "..", "..", "bin", "verilator_bin.exe")),
            os.path.normpath(os.path.join(vroot, "..", "..", "bin", "verilator_bin")),
        ])
    candidates.extend([
        os.path.normpath(os.path.join(repo_root, "..", ".tools", "oss-cad-suite",
                                      "bin", "verilator_bin.exe")),
        os.path.normpath(os.path.join(repo_root, "..", ".tools", "oss-cad-suite",
                                      "bin", "verilator_bin")),
    ])
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return shutil.which("verilator_bin") or shutil.which("verilator")


def _verilator_root(verilator):
    # The selected pinned suite is <suite>/bin/verilator_bin and its data is
    # under <suite>/share/verilator. Never trust a caller's unrelated setting.
    suite = os.path.dirname(os.path.dirname(os.path.abspath(verilator)))
    candidate = os.path.join(suite, "share", "verilator")
    return candidate if os.path.isdir(candidate) else None


def _path_identity(path):
    return os.path.normcase(os.path.normpath(os.path.abspath(path.strip('"'))))


def verilator_environment(verilator, repo_root, base_environment=None,
                          winlibs_bin=None):
    """Build the complete loader environment for one selected Verilator.

    Windows resolves this suite's DLLs through PATH.  Setting VERILATOR_ROOT
    alone therefore turns a missing loader dependency into an empty-output
    process status that can be mistaken for an RTL failure.  Derive every suite
    directory from the absolute executable and prepend the documented
    ``suite/bin; suite/lib; winlibs/bin`` order exactly once.
    """
    environment = dict(os.environ if base_environment is None else base_environment)
    selected = os.path.abspath(str(verilator))
    if not os.path.isfile(selected):
        raise ElaborationError(
            "Verilator loader environment invalid: selected executable is missing: %s" %
            selected)

    suite = os.path.dirname(os.path.dirname(selected))
    suite_bin = os.path.join(suite, "bin")
    suite_lib = os.path.join(suite, "lib")
    verilator_root = os.path.join(suite, "share", "verilator")
    required = (
        ("suite bin", suite_bin),
        ("suite lib", suite_lib),
        ("VERILATOR_ROOT", verilator_root),
    )
    for label, path in required:
        if not os.path.isdir(path):
            raise ElaborationError(
                "Verilator loader environment invalid: required %s directory is "
                "missing: %s" % (label, path))

    configured_winlibs = winlibs_bin or environment.get("ZHAO_WINLIBS_BIN")
    if configured_winlibs is None:
        workspace = os.path.dirname(os.path.dirname(os.path.abspath(str(repo_root))))
        candidate = os.path.join(workspace, "dsstuff", "mingw64", "bin")
        if os.name == "nt" or os.path.isdir(candidate):
            configured_winlibs = candidate

    prefixes = [suite_bin, suite_lib]
    if configured_winlibs is not None:
        configured_winlibs = os.path.abspath(str(configured_winlibs))
        if not os.path.isdir(configured_winlibs):
            raise ElaborationError(
                "Verilator loader environment invalid: configured winlibs bin "
                "directory is missing: %s" % configured_winlibs)
        prefixes.append(configured_winlibs)

    prefix_keys = {_path_identity(path) for path in prefixes}
    inherited = []
    for entry in environment.get("PATH", "").split(os.pathsep):
        if entry and _path_identity(entry) not in prefix_keys:
            inherited.append(entry)
    environment["PATH"] = os.pathsep.join(prefixes + inherited)
    environment["VERILATOR_ROOT"] = verilator_root
    return environment


def _source_set(decl, edges, declaration, root, extra_sources,
                package_sources=()):
    """Conservative input set for an exact Verilator elaboration."""
    modules = {root, declaration["root"]} | set(declaration["providers"])
    for seed in list(modules):
        modules.update(closure(edges, seed))
    sources = {os.path.abspath(decl[module])
               for module in modules if module in decl}
    sources.update(os.path.abspath(str(path)) for path in extra_sources)

    # Package-only files are invisible to the module graph. Put all discovered
    # packages first, in dependency order, so exact roots such as zhao_shell_top
    # cannot fail before ownership is observed merely because their imports were
    # omitted. Uninstantiated modules that share a package file remain irrelevant
    # to the V3Param cell census.
    packages = []
    for source in package_sources:
        absolute = os.path.abspath(str(source))
        if absolute not in packages:
            packages.append(absolute)
    package_set = set(packages)
    return packages + sorted(sources - package_set)


def elaborated_cells(root, sources, repo_root, verilator=None,
                     base_environment=None, parameter_overrides=None,
                     defines=()):
    """Return concrete ``(instance, module)`` cells from Verilator's V3Param AST."""
    parameter_overrides = parameter_overrides or {}
    verilator = verilator or find_verilator(repo_root)
    if not verilator:
        raise ElaborationError(
            "Verilator is required for ownership acceptance; no text-graph fallback exists")
    verilator = os.path.abspath(str(verilator))
    env = verilator_environment(verilator, repo_root, base_environment)

    with tempfile.TemporaryDirectory(prefix="zhao-owner-elab-") as mdir:
        command = [
            verilator,
            "--lint-only",
            "-Wno-PINMISSING",
            "--debugi-V3Param", "4",
            "--Mdir", mdir,
            "--prefix", "Vownership",
            "--top-module", root,
        ] + ["-G%s=%s" % item for item in parameter_overrides.items()] + [
            "-D%s" % define for define in defines
        ] + list(sources)
        result = subprocess.run(
            command, cwd=repo_root, env=env, capture_output=True, text=True,
            errors="replace")
    diagnostic = (result.stdout or "") + "\n" + (result.stderr or "")
    if result.returncode != 0:
        detail = "\n".join(line for line in diagnostic.splitlines() if line.strip())
        raise ElaborationError(
            "Verilator could not elaborate exact top '%s' (RC %d):\n%s" %
            (root, result.returncode, detail[-4000:]))
    if "V3Param.cpp" not in diagnostic:
        raise ElaborationError(
            "Verilator returned success without V3Param AST diagnostics; refusing a "
            "blind zero-instance result")

    return AST_CELL_RE.findall(diagnostic)


def _canonical_provider(module, candidates):
    # Parameter specialisations use ``provider__...``. No other prefix or
    # substring relationship has semantic meaning.
    for provider in candidates:
        if module == provider or module.startswith(provider + "__"):
            return provider
    return None


def check_roles(roles, decl, edges, extra_text, manifest, extra_sources=(),
                root_overrides=None, verilator=None, package_sources=()):
    """Return ``(errors, observations)`` from exact elaborated role roots."""
    root_overrides = root_overrides or {}
    errors = []
    observations = []
    annotations = explicit_source_annotations(decl, extra_text)
    repo_root = _repo_root(manifest)

    unknown_annotation_roles = sorted(set(annotations) - set(roles))
    for role in unknown_annotation_roles:
        errors.append("source annotation names undeclared ownership role '%s'" % role)

    for role in sorted(roles):
        declaration = roles[role]
        root = root_overrides.get(role, declaration["root"])
        registry = set(declaration["providers"])
        annotated = annotations.get(role, set())
        candidates = registry | annotated

        missing = sorted(registry - set(decl))
        if missing:
            errors.append("role '%s' declares missing provider module(s): %s" %
                          (role, ", ".join(missing)))
        if root not in decl:
            errors.append("role '%s' root '%s' is not a module" % (role, root))
            continue

        sources = _source_set(
            decl, edges, declaration, root, extra_sources, package_sources)
        try:
            cells = elaborated_cells(root, sources, repo_root, verilator)
        except ElaborationError as exc:
            errors.append("role '%s' elaboration failed: %s" % (role, exc))
            continue

        elaborated_modules = [root] + [module for _instance, module in cells]
        reachable = []
        for module in elaborated_modules:
            provider = _canonical_provider(module, candidates)
            if provider is not None:
                reachable.append(provider)

        unregistered = sorted((annotated - registry) & set(reachable))
        if unregistered:
            errors.append(
                "role '%s' has elaborated explicitly annotated provider(s) absent "
                "from its pinned registry: %s" % (role, ", ".join(unregistered)))
        observations.append((role, declaration["scope"], root, sorted(reachable)))
        if len(reachable) != 1:
            errors.append(
                "role '%s' at %s root '%s' has %d elaborated lifecycle-owner "
                "instances; expected exactly one, found [%s]" %
                (role, declaration["scope"], root, len(reachable),
                 ", ".join(sorted(reachable))))
    return errors, observations


def run_check(manifest=DEFAULT_MANIFEST, rtl_dir=DEFAULT_RTL,
              extra_sources=(), root_overrides=None, verilator=None):
    roles = read_roles(manifest)
    decl, edges, extra_text = module_edges(rtl_dir, extra_sources)
    package_sources = ordered_package_sources(rtl_dir)
    return check_roles(
        roles, decl, edges, extra_text, manifest, extra_sources,
        root_overrides, verilator, package_sources)


def _parse_override(value):
    if "=" not in value:
        raise argparse.ArgumentTypeError("root override must be ROLE=MODULE")
    role, module = value.split("=", 1)
    if not role or not module:
        raise argparse.ArgumentTypeError("root override must be ROLE=MODULE")
    return role, module


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    parser.add_argument("--rtl-dir", default=DEFAULT_RTL)
    parser.add_argument("--extra-source", action="append", default=[])
    parser.add_argument("--root", action="append", default=[], type=_parse_override,
                        metavar="ROLE=MODULE")
    parser.add_argument("--verilator", default=None)
    args = parser.parse_args(argv)
    overrides = dict(args.root)

    try:
        errors, observations = run_check(
            args.manifest, args.rtl_dir, args.extra_source, overrides, args.verilator)
    except (OSError, RoleManifestError) as exc:
        print("ownership role check failed: %s" % exc)
        return 1

    for role, scope, root, reachable in observations:
        print("ownership role %s: evidence=verilator-v3param-ast "
              "provider_identification=explicit_registry scope=%s root=%s "
              "elaborated=[%s]" %
              (role, scope, root, ", ".join(reachable)))
    if errors:
        print("ownership role check FAILED -- %d error(s)" % len(errors))
        for error in errors:
            print("  - " + error)
        return 1
    print("ownership role check OK -- every exact elaborated root has one explicit provider")
    return 0


if __name__ == "__main__":
    sys.exit(main())
