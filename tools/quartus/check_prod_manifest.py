#!/usr/bin/env python3
"""Every module under fpga/rtl is counted once, or declared absent with a reason.

The failure this guards is not "a wrong total". It is a block that is silently
NEITHER counted nor declared missing -- which reads as a healthy number and is
actually an incomplete one. So the check is exhaustive by construction: top +
inside + excluded must equal the module list exactly, in both directions.

`inside` is VERIFIED against the instantiation graph rather than trusted. A
module declared inside something that does not instantiate it would be dropped
from the count while looking accounted for.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from module_graph import build, strip_comments  # noqa: E402
from check_ownership_roles import (  # noqa: E402
    ElaborationError,
    RoleManifestError,
    run_check as check_ownership_roles,
)

MANIFEST = "design/prod_manifest.yml"
TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(TOOL_DIR, "..", ".."))
PROD_TOP_GENERATOR = os.path.join(TOOL_DIR, "gen_prod_top.py")


def _read_lines(path):
    with io.open(path, encoding="utf-8") as stream:
        return list(stream)


def read_manifest(path=MANIFEST):
    tops, excluded = [], {}
    section = None
    for raw in _read_lines(path):
        line = raw.split("#")[0].rstrip()
        if not line.strip():
            continue
        if re.match(r"^\w[\w_]*:", line):
            section = line.split(":")[0]
            continue
        m = re.match(r"^\s*-\s*(\S+)\s*$", line)
        if m and section == "top":
            name = m.group(1)
            if name in tops:
                raise ManifestSchemaError(
                    "selected top '%s' is listed more than once" % name)
            tops.append(name)
            continue
        m = re.match(r"^\s*-\s*(\S+)\s*:\s*(\S+)\s*(.*)$", line)
        if m and section == "excluded":
            name = m.group(1)
            if name in excluded:
                raise ManifestSchemaError(
                    "excluded module '%s' is listed more than once" % name)
            excluded[name] = (m.group(2), m.group(3).strip())
    overlap = sorted(set(tops) & set(excluded))
    if overlap:
        raise ManifestSchemaError(
            "module(s) appear in both top and excluded: " + ", ".join(overlap))
    return tops, excluded


def read_list_section(section_name, path=MANIFEST):
    """Read one top-level manifest section containing bare list entries.

    `retired_census_slots` is metadata, not a second accounting disposition: its
    rows reserve generated private-stimulus ordinals while the module itself
    remains accounted exactly once under `excluded`. Keeping this parser narrow
    prevents nested role-provider lists from being mistaken for tombstones.
    """
    values = []
    section = None
    for raw in _read_lines(path):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        top = re.match(r"^(\w[\w_]*):\s*$", line)
        if top:
            section = top.group(1)
            continue
        if section == section_name:
            match = re.match(r"^\s{2}-\s*(\S+)\s*$", line)
            if match:
                values.append(match.group(1))
    return values


class ManifestSchemaError(ValueError):
    """The production manifest contains unsafe or ambiguous generator input."""


# Overrides are emitted into SystemVerilog verbatim, so the accepted scalar is
# deliberately much narrower than YAML: one canonical, sized, nonnegative
# integral literal.  No expressions, signs, X/Z digits, quoting or underscores.
_CANONICAL_SV_LITERAL = re.compile(
    r"^([1-9][0-9]*)'(b[01]+|o[0-7]+|d(?:0|[1-9][0-9]*)|h[0-9A-F]+)$"
)


def _validate_canonical_sv_literal(value):
    match = _CANONICAL_SV_LITERAL.fullmatch(value)
    if not match:
        raise ManifestSchemaError(
            "noncanonical parameter override value %r; expected a sized literal "
            "such as 1'b0, 12'd7 or 8'hFF" % value
        )
    width = int(match.group(1))
    if width > 4096:
        raise ManifestSchemaError(
            "parameter override width %d exceeds the closed-schema limit of 4096"
            % width
        )
    payload = match.group(2)
    base_name, digits = payload[0], payload[1:]
    base = {"b": 2, "o": 8, "d": 10, "h": 16}[base_name]
    number = int(digits, base)
    if number >= (1 << width):
        raise ManifestSchemaError(
            "parameter override value %r does not fit its declared %d-bit width"
            % (value, width)
        )
    if base_name == "b":
        canonical_digits = format(number, "0%db" % width)
    elif base_name == "o":
        canonical_digits = format(number, "0%do" % ((width + 2) // 3))
    elif base_name == "h":
        canonical_digits = format(number, "0%dX" % ((width + 3) // 4))
    else:
        canonical_digits = str(number)
    canonical = "%d'%s%s" % (width, base_name, canonical_digits)
    if value != canonical:
        raise ManifestSchemaError(
            "noncanonical parameter override value %r; canonical spelling is %s"
            % (value, canonical)
        )
    return {"width": width, "value": number, "text": value}


def read_parameter_overrides(path=MANIFEST):
    """Read the closed ``production_parameter_overrides`` manifest mapping.

    Its only accepted shape is::

        production_parameter_overrides:
          selected_module:
            PARAMETER_NAME: 1'b0

    Missing means no overrides, preserving the historical default-only output.
    Once the section is present, every non-comment row must match that shape;
    permissive YAML coercions must never become emitted SystemVerilog.
    """
    overrides = {}
    in_section = False
    saw_section = False
    current_top = None
    current_count = 0

    def finish_top():
        if current_top is not None and current_count == 0:
            raise ManifestSchemaError(
                "production parameter override top '%s' has no parameter entries"
                % current_top
            )

    for line_number, raw in enumerate(_read_lines(path), 1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue

        if line == "production_parameter_overrides:":
            if saw_section:
                raise ManifestSchemaError(
                    "duplicate production_parameter_overrides section at line %d"
                    % line_number
                )
            finish_top()
            saw_section = True
            in_section = True
            current_top = None
            current_count = 0
            continue
        if re.match(r"^\s*production_parameter_overrides\b", line):
            raise ManifestSchemaError(
                "malformed production_parameter_overrides declaration at line %d"
                % line_number
            )

        if in_section and re.fullmatch(r"[A-Za-z_]\w*:\s*", line):
            finish_top()
            if not overrides:
                raise ManifestSchemaError(
                    "production_parameter_overrides section has no top entries"
                )
            in_section = False
            current_top = None
            current_count = 0
            # This is the next ordinary top-level manifest section.
            continue
        if not in_section:
            continue

        top_match = re.fullmatch(r"  ([A-Za-z_]\w*):", line)
        if top_match:
            finish_top()
            current_top = top_match.group(1)
            current_count = 0
            if current_top in overrides:
                raise ManifestSchemaError(
                    "duplicate production parameter override top '%s' at line %d"
                    % (current_top, line_number)
                )
            overrides[current_top] = {}
            continue

        parameter_match = re.fullmatch(
            r"    ([A-Za-z_]\w*): ([^\s]+)", line
        )
        if parameter_match and current_top is not None:
            name, value = parameter_match.groups()
            if name in overrides[current_top]:
                raise ManifestSchemaError(
                    "duplicate override for parameter '%s.%s' at line %d"
                    % (current_top, name, line_number)
                )
            _validate_canonical_sv_literal(value)
            overrides[current_top][name] = value
            current_count += 1
            continue

        raise ManifestSchemaError(
            "malformed production_parameter_overrides row at line %d: %s"
            % (line_number, line.strip())
        )

    if in_section:
        finish_top()
        if not overrides:
            raise ManifestSchemaError(
                "production_parameter_overrides section has no top entries"
            )
    return overrides


def _split_top_level_commas(text):
    """Split a declaration list without splitting nested constant expressions."""
    parts = []
    start = 0
    stack = []
    pairs = {")": "(", "]": "[", "}": "{"}
    for index, character in enumerate(text):
        if character in "([{":
            stack.append(character)
        elif character in pairs:
            if stack and stack[-1] == pairs[character]:
                stack.pop()
        elif character == "," and not stack:
            parts.append(text[start:index])
            start = index + 1
    parts.append(text[start:])
    return parts


def _module_parameter_block(text, module):
    """Return ``module``'s parameter-port text, or ``None``."""
    body = strip_comments(text)
    match = re.search(r"\bmodule\s+" + re.escape(module) + r"\b", body)
    if not match:
        return None
    suffix = body[match.end():]
    while True:
        imported = re.match(r"\s*import\b[^;]*;", suffix)
        if imported is None:
            break
        suffix = suffix[imported.end():]
    marker = re.match(r"\s*#\s*\(", suffix)
    if not marker:
        return None
    opening = suffix.index("(", marker.start())
    depth = 0
    for index in range(opening, len(suffix)):
        if suffix[index] == "(":
            depth += 1
        elif suffix[index] == ")":
            depth -= 1
            if depth == 0:
                return suffix[opening + 1:index]
    return None


def _sv_default_number(token):
    """One integral SV literal used while resolving declaration widths."""
    token = token.strip()
    match = re.fullmatch(
        r"(\d+)?'([sS]?)([dDhHbBoO])([0-9a-fA-F_]+)", token
    )
    if not match:
        return int(token)
    width_text, signed_mark, base_name, digits = match.groups()
    base = {"d": 10, "h": 16, "b": 2, "o": 8}[base_name.lower()]
    value = int(digits.replace("_", ""), base)
    if width_text and signed_mark and value & (1 << (int(width_text) - 1)):
        value -= 1 << int(width_text)
    return value


def _eval_parameter_expr(expression, values):
    """Evaluate the small integral-expression subset used by packed widths."""
    cooked = expression.strip()
    cooked = re.sub(r"\$clog2", "clog2", cooked)
    cooked = re.sub(
        r"(?:\d+)?'[sS]?[dDhHbBoO][0-9a-fA-F_]+",
        lambda match: str(_sv_default_number(match.group(0))),
        cooked,
    )
    for name in sorted(values, key=len, reverse=True):
        cooked = re.sub(
            r"(?<![\w$])" + re.escape(name) + r"(?![\w])",
            str(values[name]),
            cooked,
        )
    if re.search(r"[A-Za-z_]", cooked.replace("clog2", "")):
        raise ValueError("unresolved parameter expression: " + cooked)
    value = eval(
        cooked,
        {"clog2": lambda item: (0 if int(item) <= 1 else
                                  (int(item) - 1).bit_length()),
         "__builtins__": {}},
        {},
    )
    return int(value)


def _parameter_type_shape(type_text, unpacked_text, values):
    """Classify one declared value parameter's integral coercion shape."""
    result = {
        "kind": "value",
        "type": type_text.strip() or "implicit",
        "integral": False,
        "signed": None,
        "width": None,
        "reason": None,
    }
    if unpacked_text.strip():
        result["reason"] = "unpacked-array parameters are not scalar integral values"
        return result

    ranges = re.findall(r"\[[^\]]+\]", type_text)
    words_text = re.sub(r"\[[^\]]+\]", " ", type_text)
    words = re.findall(r"[A-Za-z_]\w*", words_text)
    explicit_signed = "signed" in words
    explicit_unsigned = "unsigned" in words
    qualifiers = {"signed", "unsigned", "var", "const"}
    base_words = [word for word in words if word not in qualifiers]

    fixed = {
        "bit": (1, False),
        "logic": (1, False),
        "reg": (1, False),
        "byte": (8, True),
        "shortint": (16, True),
        "int": (32, True),
        "longint": (64, True),
        "integer": (32, True),
        "time": (64, False),
    }
    rejected = {
        "string", "real", "shortreal", "realtime", "chandle", "event",
    }
    if len(base_words) == 1 and base_words[0] in rejected:
        result["reason"] = "declared %s parameter is non-integral" % base_words[0]
        return result
    if len(base_words) == 1 and base_words[0] in fixed:
        base_width, default_signed = fixed[base_words[0]]
    elif not base_words and ranges:
        # A packed range without a base keyword is an implicit logic vector.
        base_width, default_signed = 1, False
        result["type"] = type_text.strip() or "logic"
    else:
        result["reason"] = (
            "parameter type %r is implicit or not a supported built-in integral "
            "type" % (type_text.strip() or "<implicit>")
        )
        return result

    if explicit_signed and explicit_unsigned:
        result["reason"] = "parameter declaration is both signed and unsigned"
        return result
    signed = (True if explicit_signed else False if explicit_unsigned
              else default_signed)
    width = base_width
    try:
        for packed_range in ranges:
            inner = packed_range[1:-1]
            if ":" not in inner:
                raise ValueError("packed dimension has no range")
            high, low = inner.split(":", 1)
            width *= abs(
                _eval_parameter_expr(high, values)
                - _eval_parameter_expr(low, values)
            ) + 1
    except (TypeError, ValueError, SyntaxError, ZeroDivisionError) as exc:
        result["reason"] = "packed width is unresolved (%s)" % exc
        return result
    if width <= 0 or width > 4096:
        result["reason"] = "effective packed width %d is outside 1..4096" % width
        return result

    result.update({
        "integral": True,
        "signed": signed,
        "width": width,
        "reason": None,
    })
    return result


def module_parameter_declarations(text, module, effective_values=None):
    """Declared parameter metadata keyed by name, in declaration order.

    Each row records value-vs-type kind, declared type text, integral eligibility,
    signedness and effective scalar/packed width. A declaration sees only defaults
    and validated overrides from PRECEDING declarations. Forward and cyclic width
    references therefore stay unresolved and fail closed instead of borrowing a
    later value that SystemVerilog had not declared at that point.
    """
    block = _module_parameter_block(text, module)
    if block is None:
        return {}

    raw_declarations = []
    current_type = None
    current_kind = None
    for declaration in _split_top_level_commas(block):
        item = declaration.strip()
        starts_parameter = re.match(r"^parameter\b", item) is not None
        if starts_parameter:
            item = re.sub(r"^parameter\b", "", item, count=1).strip()
        if "=" not in item:
            continue
        lhs, default = item.split("=", 1)
        blanked = re.sub(r"\[[^\]]+\]", lambda match: " " * len(match.group(0)), lhs)
        identifiers = list(re.finditer(r"[A-Za-z_]\w*", blanked))
        if not identifiers:
            continue
        name_match = identifiers[-1]
        name = name_match.group(0)
        prefix = lhs[:name_match.start()].strip()
        suffix = lhs[name_match.end():].strip()

        if starts_parameter:
            if re.match(r"^type\b", prefix):
                current_kind = "type"
                current_type = "type"
            else:
                current_kind = "value"
                current_type = prefix
        elif current_type is None:
            # A continuation without a preceding `parameter` is malformed source;
            # retain a refusal row rather than inventing declaration semantics.
            current_kind = "value"
            current_type = ""
        elif prefix:
            # Be conservative if a source uses an explicit type on a continuation.
            current_kind = "value"
            current_type = prefix

        raw_declarations.append({
            "name": name,
            "kind": current_kind,
            "type_text": current_type or "",
            "unpacked_text": suffix,
            "default": default.strip(),
        })

    selected_values = effective_values or {}
    preceding_values = {}
    result = {}
    for declaration in raw_declarations:
        name = declaration["name"]
        if declaration["kind"] == "type":
            metadata = {
                "kind": "type",
                "type": "type",
                "integral": False,
                "signed": None,
                "width": None,
                "reason": "type parameters cannot accept integral literal overrides",
            }
        else:
            metadata = _parameter_type_shape(
                declaration["type_text"],
                declaration["unpacked_text"],
                preceding_values,
            )
        result[name] = metadata

        # Only an integral value whose declaration has already been understood may
        # enter the environment seen by later packed ranges. `effective_values`
        # contains validator-produced integers only; raw manifest literals never
        # reach this path.
        if metadata["kind"] != "value" or not metadata["integral"]:
            continue
        if name in selected_values:
            preceding_values[name] = selected_values[name]
            continue
        try:
            preceding_values[name] = _eval_parameter_expr(
                declaration["default"], preceding_values
            )
        except Exception:
            # A later declaration that depends on this name will carry an explicit
            # unresolved-width refusal. Do not guess from a forward declaration.
            pass
    return result


def module_parameter_names(text, module):
    """Return names declared in ``module``'s ``#(...)`` parameter port list."""
    return set(module_parameter_declarations(text, module))


def _coerce_parameter_override(top, name, value, declaration):
    """Return the exact destination value, or reject lossy SV coercion."""
    qualified = "%s.%s" % (top, name)
    if declaration["kind"] != "value":
        raise ManifestSchemaError(
            "override parameter '%s' is a type parameter, not an integral value"
            % qualified
        )
    if not declaration["integral"]:
        raise ManifestSchemaError(
            "override parameter '%s' is not a supported integral scalar (%s)"
            % (qualified, declaration["reason"] or declaration["type"])
        )

    literal = _validate_canonical_sv_literal(value)
    width = declaration["width"]
    unsigned_bits = literal["value"] & ((1 << width) - 1)
    if declaration["signed"] and unsigned_bits & (1 << (width - 1)):
        coerced = unsigned_bits - (1 << width)
    else:
        coerced = unsigned_bits
    if coerced != literal["value"]:
        signed_word = "signed" if declaration["signed"] else "unsigned"
        raise ManifestSchemaError(
            "override value %s for '%s' is not representable as the declared "
            "%d-bit %s integral parameter; SystemVerilog coercion would produce %d"
            % (value, qualified, width, signed_word, coerced)
        )
    return coerced


def validate_parameter_overrides(overrides, tops, declarations,
                                 repo_root=REPO_ROOT):
    """Validate declarations and return exact coerced integer values."""
    selected = set(tops)
    errors = []
    coerced_overrides = {}
    for top, parameters in overrides.items():
        if top not in selected:
            errors.append("override top '%s' is not present in top:" % top)
            continue
        source = declarations.get(top)
        if source is None:
            errors.append("override top '%s' has no module declaration" % top)
            continue
        source_path = os.fspath(source)
        if not os.path.isabs(source_path):
            source_path = os.path.join(os.fspath(repo_root), source_path)
        try:
            with io.open(source_path, encoding="utf-8",
                         errors="replace") as stream:
                text = stream.read()
        except OSError as exc:
            errors.append("cannot read declaration for override top '%s': %s" %
                          (top, exc))
            continue
        declared = module_parameter_declarations(text, top)
        for parameter in parameters:
            if parameter not in declared:
                errors.append(
                    "override parameter '%s.%s' is not declared in the module's "
                    "parameter port list" % (top, parameter)
                )

        # Declaration order is semantic. A packed type may depend on an earlier
        # parameter, and it must see that parameter's VALIDATED override rather
        # than its default. Manifest mapping order neither creates nor repairs a
        # dependency; forward/cyclic ranges remain unresolved refusal metadata.
        top_values = {}
        for parameter in declared:
            if parameter not in parameters:
                continue
            effective_declared = module_parameter_declarations(
                text, top, effective_values=top_values
            )
            try:
                coerced = _coerce_parameter_override(
                    top, parameter, parameters[parameter],
                    effective_declared[parameter],
                )
            except ManifestSchemaError as exc:
                errors.append(str(exc))
            else:
                top_values[parameter] = coerced
                coerced_overrides.setdefault(top, {})[parameter] = coerced
    if errors:
        raise ManifestSchemaError("; ".join(errors))
    return coerced_overrides


def module_edges():
    decl, inst = build()
    fmods = {}
    for m, p in decl.items():
        fmods.setdefault(p, []).append(m)
    edges = {}
    for p, ms in inst.items():
        for owner in fmods[p]:
            edges.setdefault(owner, set()).update(ms)
    return decl, edges


def closure(edges, root):
    seen, stack = set(), [root]
    while stack:
        for y in edges.get(stack.pop(), ()):
            if y not in seen:
                seen.add(y)
                stack.append(y)
    return seen



def generated_top_direct_modules(text):
    """Modules instantiated directly by the generated accounting top.

    The optional parameter block is part of the instance declaration.  Omitting
    it from this recognizer would make every explicitly overridden top invisible
    to the fit-source closure check precisely when override support is used.
    """
    return set(re.findall(
        r"^\s{2}(zhao_\w+)\s*(?:#\s*\((?:[^;]*?)\)\s*)?"
        r"u\d+_i\s*\(",
        text,
        re.M,
    ))


def check_fit_sources(decl, edges=None,
                      top_path="fpga/rtl/prod/zhao_prod_top.sv",
                      yml="design/fit_targets.yml"):
    """Every module in the GENERATED production top's INSTANTIATION CLOSURE must
    appear in zhao_prod_top's source list in design/fit_targets.yml.

    CLOSURE, not direct instances -- and that word is the 2026-09-09 repair.

    This function used to scan only what `zhao_prod_top.sv` instantiates by name.
    It therefore could not see `zhao_skid2`, which is instantiated by
    `zhao_raster_tile_pipe`, one level down. The production source list was
    missing it, the fit would have died at elaboration, and this checker reported
    OK. Verilator found it instead, which is the wrong tool discovering the thing
    this one exists for.

    A gate that inspects one level of a hierarchy is not a gate on the
    hierarchy -- and it reads exactly like one, which is the expensive part.
    """
    import os
    import re

    out = []
    if not os.path.exists(top_path):
        out.append("generated production top is missing: %s" % top_path)
    if not os.path.exists(yml):
        out.append("production fit-target manifest is missing: %s" % yml)
    if out:
        return out

    with io.open(yml, encoding="utf-8", errors="replace") as stream:
        y = stream.read()
    active_y = "\n".join(line.split("#", 1)[0] for line in y.splitlines())
    i = active_y.find("- top: zhao_prod_top")
    if i < 0:
        return ["design/fit_targets.yml has no zhao_prod_top target, so the "
                "production fit cannot be run at all"]
    seg = active_y[i:]
    j = seg.find("\n  - top:")
    if j > 0:
        seg = seg[:j]
    active_lines = [line.split("#", 1)[0].rstrip()
                    for line in seg.splitlines()]
    listed = set()
    for line in active_lines:
        match = re.fullmatch(r"\s+-\s+(fpga/rtl/\S+\.sv)\s*", line)
        if match:
            listed.add(match.group(1))

    with io.open(top_path, encoding="utf-8", errors="replace") as stream:
        top = stream.read()
    direct = (set(edges.get("zhao_prod_top", ())) if edges is not None
              else generated_top_direct_modules(top))

    # Expand to the full closure. Every module reachable from anything the top
    # instantiates has to be compiled too, or elaboration stops -- and Quartus
    # reports that as a missing module, which reads like a typo rather than a
    # source-list gap.
    inst = set(direct)
    if edges:
        for m in direct:
            inst |= closure(edges, m)

    for m in sorted(inst):
        src = decl.get(m)
        if src is None:
            out.append(
                "the generated production top instantiates '%s' and no such "
                "module file exists" % m)
        elif src not in listed:
            # Say WHERE it is instantiated. "instantiated by the generated
            # production top" was the old message, and for a transitively-reached
            # module it is simply false -- zhao_skid2 is instantiated by
            # zhao_raster_tile_pipe. A gate that misreports the location sends
            # the reader to the wrong file.
            how = ("instantiated directly by the generated production top"
                   if m in direct else
                   "reachable from the generated production top (instantiated by "
                   "a submodule, not by the top itself)")
            out.append(
                "'%s' (%s) is %s but is NOT in zhao_prod_top's source list in "
                "design/fit_targets.yml -- the fit would die at elaboration"
                % (m, src, how))
    return out


def check_top_fresh(command=None):
    """Require the generator's complete ``--check`` contract to succeed.

    RC 1 means at least one selected top was skipped, RC 2 is a refusal to
    generate, and RC 3 is a stale/missing output. Every nonzero result invalidates
    the generated accounting hierarchy; treating only RC 3 as failure turns a
    skipped module or a generator refusal into false freshness.

    ``command`` exists for committed executable status controls. Production use
    always invokes the real generator.
    """
    import subprocess
    import sys as _sys
    if command is None:
        if not os.path.exists(PROD_TOP_GENERATOR):
            return ["generator freshness command is missing: %s" %
                    PROD_TOP_GENERATOR]
        command = [_sys.executable, PROD_TOP_GENERATOR, "--check"]
    try:
        # gen_prod_top's manifest, RTL and output paths are intentionally
        # repository-relative. Anchor its process at the source root rather than
        # inheriting CTest's binary-directory cwd.
        result = subprocess.run(command, cwd=REPO_ROOT,
                                capture_output=True, text=True)
    except OSError as exc:
        return ["could not run generator freshness command (%s)" % exc]
    if result.returncode == 0:
        return []

    diagnostic = "\n".join(
        line for line in ((result.stdout or "") + "\n" +
                          (result.stderr or "")).splitlines() if line.strip())
    meanings = {
        1: "selected top skipped",
        2: "generator refusal",
        3: "stale or mismatched generated top",
    }
    meaning = meanings.get(result.returncode, "generator failure")
    detail = ("; output: " + diagnostic[-2000:]) if diagnostic else ""
    return [
        "fpga/rtl/prod/zhao_prod_top.sv freshness is invalid: generator "
        "--check returned RC %d (%s)%s" %
        (result.returncode, meaning, detail)
    ]


def main():
    decl, edges = module_edges()
    try:
        tops, excluded = read_manifest()
    except ManifestSchemaError as exc:
        print("MANIFEST CHECK FAILED -- production disposition schema is invalid")
        print("  - " + str(exc))
        return 1
    retired_slots = read_list_section("retired_census_slots")
    errors = []
    try:
        parameter_overrides = read_parameter_overrides()
    except ManifestSchemaError as exc:
        parameter_overrides = {}
        errors.append("production parameter override schema is invalid: %s" % exc)
    else:
        try:
            validate_parameter_overrides(parameter_overrides, tops, decl)
        except ManifestSchemaError as exc:
            errors.append("production parameter overrides are invalid: %s" % exc)

    duplicates = sorted({name for name in retired_slots
                         if retired_slots.count(name) > 1})
    for name in duplicates:
        errors.append("retired census slot '%s' is listed more than once" % name)
    for name in retired_slots:
        if name not in decl:
            errors.append("retired census slot '%s' is not a module under fpga/rtl" % name)
        if name in tops:
            errors.append("retired census slot '%s' is still a selected top" % name)
        if name not in excluded or excluded[name][0] != "superseded":
            errors.append(
                "retired census slot '%s' must be accounted exactly once as "
                "excluded:superseded" % name)

    try:
        role_errors, role_observations = check_ownership_roles()
        errors.extend(role_errors)
    except (OSError, RoleManifestError, ElaborationError) as exc:
        role_observations = []
        errors.append("ownership role declaration is invalid: %s" % exc)

    for t in tops:
        if t not in decl:
            errors.append("top '%s' is not a module under fpga/rtl" % t)
    for e in excluded:
        if e not in decl:
            errors.append("excluded '%s' is not a module under fpga/rtl" % e)

    inside = {}
    for t in tops:
        if t not in decl:
            continue
        for m in closure(edges, t):
            inside.setdefault(m, []).append(t)

    # A top that is also inside another top would be counted twice -- once
    # standalone and once within its parent. That is the specific arithmetic
    # error this whole exercise exists to remove.
    for t in tops:
        if t in inside:
            errors.append(
                "DOUBLE COUNT: top '%s' is already instantiated by %s"
                % (t, ", ".join(inside[t]))
            )

    for e, (reason, _d) in excluded.items():
        if e in inside:
            errors.append(
                "excluded '%s' (%s) is nevertheless instantiated by %s -- it is "
                "in the machine whatever the manifest says"
                % (e, reason, ", ".join(inside[e]))
            )

    # ---- the SECOND place a new block has to be registered -----------------
    # A block being in the manifest is not enough for the production fit to
    # elaborate: design/fit_targets.yml carries zhao_prod_top's own flat source
    # list, and nothing connected the two. On 2026-09-04 zhao_geom_assemble and
    # zhao_geom_depthquant were caught here as UNACCOUNTED, added to the
    # manifest, regenerated into the top -- and the fit would STILL have died at
    # elaboration, because neither was in that source list.
    #
    # The manifest gate and the fit source list were one step apart and only
    # the first of them was mechanical. This closes the gap: whatever the
    # generated top instantiates must be compilable.
    errors.extend(check_fit_sources(decl, edges))
    errors.extend(check_top_fresh())

    accounted = set(tops) | set(inside) | set(excluded)
    for m in sorted(set(decl) - accounted):
        errors.append(
            "UNACCOUNTED: %s (%s) is neither counted nor declared absent"
            % (m, decl[m].replace("fpga/rtl/", ""))
        )

    for role, scope, root, reachable in role_observations:
        print("ownership role %s: evidence=verilator-v3param-ast "
              "provider_identification=explicit_registry scope=%s root=%s "
              "elaborated=[%s]" %
              (role, scope, root, ", ".join(reachable)))
    print(
        "prod manifest: %d modules, %d tops, %d inside, %d excluded, "
        "%d retired census slots"
        % (len(decl), len(tops), len(inside), len(excluded), len(retired_slots))
    )
    if errors:
        print("\nMANIFEST CHECK FAILED -- %d error(s)" % len(errors))
        for e in errors:
            print("  - " + e)
        return 1
    print("manifest check OK -- every module counted once or declared absent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
