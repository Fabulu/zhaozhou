#!/usr/bin/env python3
"""Side-effect-free SystemVerilog shell-port declaration and policy model.

This module deliberately performs no filesystem access at import time.  Callers
supply source/policy text and receive immutable models or precise diagnostics.
It parses ANSI module declarations; it is not a general SystemVerilog parser.
The generated fit-top flow independently checks these results against Verilator
elaboration, so a plausible parser undercount cannot certify itself.
"""

from __future__ import annotations

import ast
from dataclasses import dataclass
import hashlib
import json
import operator
import re
from typing import Iterable, Mapping, Sequence


class ShellPortError(ValueError):
    """A declaration or policy is incomplete, ambiguous, or inconsistent."""


@dataclass(frozen=True)
class Dimension:
    text: str
    left_text: str
    right_text: str
    left: int
    right: int

    @property
    def size(self) -> int:
        return abs(self.left - self.right) + 1

    @property
    def indices(self) -> tuple[int, ...]:
        step = 1 if self.right >= self.left else -1
        return tuple(range(self.left, self.right + step, step))


@dataclass(frozen=True)
class Port:
    ordinal: int
    name: str
    direction: str
    type_text: str
    signal_type_text: str
    packed_dimensions: tuple[Dimension, ...]
    unpacked_dimensions: tuple[Dimension, ...]
    signed: bool
    element_width: int
    bit_width: int

    @property
    def unpacked_count(self) -> int:
        count = 1
        for dim in self.unpacked_dimensions:
            count *= dim.size
        return count


@dataclass(frozen=True)
class ModuleDeclaration:
    module_name: str
    declaration_text: str
    declaration_sha256: str
    ports: tuple[Port, ...]

    @property
    def total_bits(self) -> int:
        return sum(port.bit_width for port in self.ports)

    @property
    def input_bits(self) -> int:
        return sum(port.bit_width for port in self.ports if port.direction == "input")

    @property
    def output_bits(self) -> int:
        return sum(port.bit_width for port in self.ports if port.direction == "output")


@dataclass(frozen=True)
class PolicyPort:
    ordinal: int
    name: str
    direction: str
    domain: str
    driver: str | None
    sink: str | None
    dynamic_mask: int | None
    constant_reason: str | None


@dataclass(frozen=True)
class ShellPolicy:
    schema_version: int
    module: str
    traffic_profile: str
    ports: tuple[PolicyPort, ...]
    raw_sha256: str


_IDENT_RE = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")
_DIRECTION_RE = re.compile(r"^(input|output|inout)\b")
_STORAGE_WORDS = {"var", "wire", "reg"}
_SIGN_WORDS = {"signed", "unsigned"}
_BUILTIN_WIDTHS = {
    "logic": 1,
    "bit": 1,
    "reg": 1,
    "byte": 8,
    "shortint": 16,
    "int": 32,
    "integer": 32,
    "longint": 64,
    "time": 64,
}
_ALLOWED_DOMAINS = {"external", "gpu", "video", "audio"}
_ALLOWED_DRIVERS = {
    "top_port",
    "frame_ring",
    "hps_responder",
    "pads",
    "audio_producer",
    "counter_consumer",
    "render_producer",
    "geometry_guard",
    "sdr_phy_responder",
}
_ALLOWED_SINKS = {"gpu_capture", "video_capture", "audio_capture"}


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def strip_comments(text: str) -> str:
    """Replace comments with whitespace while preserving offsets and newlines."""
    out = list(text)
    i = 0
    state = "code"
    quote = ""
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "line"
                continue
            if ch == "/" and nxt == "*":
                out[i] = out[i + 1] = " "
                i += 2
                state = "block"
                continue
            if ch in {'"', "'"}:
                # A single quote usually begins an SV literal, not a string.
                if ch == '"':
                    quote = ch
                    state = "string"
            i += 1
            continue
        if state == "line":
            if ch == "\n":
                state = "code"
            else:
                out[i] = " "
            i += 1
            continue
        if state == "block":
            if ch == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "code"
            else:
                if ch != "\n":
                    out[i] = " "
                i += 1
            continue
        if state == "string":
            if ch == "\\":
                i += 2
                continue
            if ch == quote:
                state = "code"
            i += 1
    if state == "block":
        raise ShellPortError("unterminated block comment")
    return "".join(out)


def _normalise_sv(text: str) -> str:
    text = re.sub(r"\s+", " ", text.strip())
    text = re.sub(r"\s*\[\s*", " [", text)
    text = re.sub(r"\s*\]\s*", "] ", text)
    text = re.sub(r"\s*:\s*", ":", text)
    return re.sub(r"\s+", " ", text).strip()


def _balanced_end(text: str, open_at: int, opening: str, closing: str) -> int:
    if open_at >= len(text) or text[open_at] != opening:
        raise ShellPortError(f"expected {opening!r} at declaration offset {open_at}")
    depth = 0
    for i in range(open_at, len(text)):
        ch = text[i]
        if ch == opening:
            depth += 1
        elif ch == closing:
            depth -= 1
            if depth == 0:
                return i
    raise ShellPortError(f"unterminated {opening}{closing} group")


def _skip_space(text: str, at: int) -> int:
    while at < len(text) and text[at].isspace():
        at += 1
    return at


def _module_header_span(source: str, module_name: str) -> tuple[int, int, int, int]:
    clean = strip_comments(source)
    matches = list(re.finditer(r"\bmodule\s+" + re.escape(module_name) + r"\b", clean))
    if len(matches) != 1:
        raise ShellPortError(
            f"expected exactly one module {module_name!r}, found {len(matches)}"
        )
    start = matches[0].start()
    at = matches[0].end()
    while True:
        at = _skip_space(clean, at)
        if not clean.startswith("import", at) or (
            at + 6 < len(clean) and (clean[at + 6].isalnum() or clean[at + 6] in "_$")
        ):
            break
        semi = clean.find(";", at)
        if semi < 0:
            raise ShellPortError(f"unterminated import before {module_name} port list")
        at = semi + 1
    at = _skip_space(clean, at)
    if at < len(clean) and clean[at] == "#":
        at = _skip_space(clean, at + 1)
        if at >= len(clean) or clean[at] != "(":
            raise ShellPortError(f"malformed parameter block for {module_name}")
        at = _balanced_end(clean, at, "(", ")") + 1
    at = _skip_space(clean, at)
    if at >= len(clean) or clean[at] != "(":
        raise ShellPortError(f"missing ANSI port list for {module_name}")
    port_open = at
    port_close = _balanced_end(clean, port_open, "(", ")")
    semi = _skip_space(clean, port_close + 1)
    if semi >= len(clean) or clean[semi] != ";":
        raise ShellPortError(f"missing semicolon after {module_name} declaration")
    return start, semi + 1, port_open + 1, port_close


def _split_top_level(text: str, separator: str = ",") -> list[str]:
    parts: list[str] = []
    start = 0
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in "([{":
            stack.append(ch)
        elif ch in ")]}":
            if not stack or stack[-1] != pairs[ch]:
                raise ShellPortError(f"unbalanced {ch!r} in declaration")
            stack.pop()
        elif ch == separator and not stack:
            parts.append(text[start:i])
            start = i + 1
        i += 1
    if stack:
        raise ShellPortError("unbalanced group in declaration")
    parts.append(text[start:])
    return parts


def _top_level_identifiers(text: str) -> list[tuple[str, int, int]]:
    ids: list[tuple[str, int, int]] = []
    depth = {"(": 0, "[": 0, "{": 0}
    close_to_open = {")": "(", "]": "[", "}": "{"}
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in depth:
            depth[ch] += 1
            i += 1
            continue
        if ch in close_to_open:
            key = close_to_open[ch]
            depth[key] -= 1
            if depth[key] < 0:
                raise ShellPortError(f"unbalanced {ch!r} in port fragment {text!r}")
            i += 1
            continue
        if all(value == 0 for value in depth.values()):
            match = _IDENT_RE.match(text, i)
            if match:
                ids.append((match.group(0), match.start(), match.end()))
                i = match.end()
                continue
        i += 1
    return ids


def _dimension_texts(text: str) -> tuple[str, ...]:
    dims: list[str] = []
    i = 0
    while i < len(text):
        if text[i] != "[":
            i += 1
            continue
        end = _balanced_end(text, i, "[", "]")
        dims.append(_normalise_sv(text[i : end + 1]))
        i = end + 1
    return tuple(dims)


_BIN_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.floordiv,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.LShift: operator.lshift,
    ast.RShift: operator.rshift,
    ast.BitOr: operator.or_,
    ast.BitAnd: operator.and_,
    ast.BitXor: operator.xor,
    ast.Pow: operator.pow,
}
_UNARY_OPS = {ast.UAdd: operator.pos, ast.USub: operator.neg, ast.Invert: operator.invert}


def _replace_sv_numbers(expression: str) -> str:
    pattern = re.compile(r"(?:(\d+)\s*)?'([sS]?)([dDhHbBoO])([0-9a-fA-F_xXzZ?]+)")

    def replace(match: re.Match[str]) -> str:
        digits = match.group(4).replace("_", "")
        if re.search(r"[xXzZ?]", digits):
            raise ShellPortError(f"unknown digit in constant expression {expression!r}")
        base = {"d": 10, "h": 16, "b": 2, "o": 8}[match.group(3).lower()]
        return str(int(digits, base))

    return pattern.sub(replace, expression.replace("$clog2", "clog2"))


def eval_int(expression: str, constants: Mapping[str, int] | None = None) -> int:
    """Evaluate a deliberately small, integer-only SV constant expression."""
    constants = constants or {}
    rewritten = _replace_sv_numbers(expression)
    try:
        tree = ast.parse(rewritten, mode="eval")
    except SyntaxError as exc:
        raise ShellPortError(f"unsupported constant expression {expression!r}") from exc

    def visit(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return visit(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return int(node.value)
        if isinstance(node, ast.Name):
            if node.id not in constants:
                raise ShellPortError(
                    f"unresolved identifier {node.id!r} in constant expression {expression!r}"
                )
            return int(constants[node.id])
        if isinstance(node, ast.BinOp) and type(node.op) in _BIN_OPS:
            return int(_BIN_OPS[type(node.op)](visit(node.left), visit(node.right)))
        if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARY_OPS:
            return int(_UNARY_OPS[type(node.op)](visit(node.operand)))
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            if node.func.id == "clog2" and len(node.args) == 1:
                value = visit(node.args[0])
                if value <= 0:
                    raise ShellPortError("$clog2 argument must be positive")
                return (value - 1).bit_length()
        raise ShellPortError(f"unsupported constant expression {expression!r}")

    return visit(tree)


def _parse_dimension(text: str, constants: Mapping[str, int]) -> Dimension:
    inner = text.strip()[1:-1].strip()
    pieces = _split_top_level(inner, ":")
    if len(pieces) != 2:
        raise ShellPortError(f"dimension {text!r} is not an explicit [left:right] range")
    left_text, right_text = (piece.strip() for piece in pieces)
    left = eval_int(left_text, constants)
    right = eval_int(right_text, constants)
    return Dimension(_normalise_sv(text), left_text, right_text, left, right)


def _base_type(type_text: str) -> str:
    without_dims = re.sub(r"\[[^\]]*\]", " ", type_text)
    words = _IDENT_RE.findall(without_dims)
    filtered = [
        word
        for word in words
        if word not in _STORAGE_WORDS and word not in _SIGN_WORDS and word != "const"
    ]
    if not filtered:
        return "logic"
    # ANSI declarations in this flow use one scalar builtin or one typedef.
    return filtered[-1]


def _signal_type(type_text: str) -> str:
    words = type_text.split()
    while words and words[0] in _STORAGE_WORDS:
        words.pop(0)
    result = " ".join(words).strip()
    return result or "logic"


def discover_module_parameters(
    source: str,
    module_name: str,
    *,
    constants: Mapping[str, int] | None = None,
) -> dict[str, int]:
    """Resolve integer defaults from one module's ``#(...)`` block."""
    clean = strip_comments(source)
    match = re.search(r"\bmodule\s+" + re.escape(module_name) + r"\b", clean)
    if not match:
        raise ShellPortError(f"module {module_name!r} is absent")
    at = match.end()
    while True:
        at = _skip_space(clean, at)
        if not clean.startswith("import", at):
            break
        semi = clean.find(";", at)
        if semi < 0:
            raise ShellPortError(f"unterminated import before {module_name} parameters")
        at = semi + 1
    at = _skip_space(clean, at)
    values = dict(constants or {})
    if at >= len(clean) or clean[at] != "#":
        return values
    at = _skip_space(clean, at + 1)
    if at >= len(clean) or clean[at] != "(":
        raise ShellPortError(f"malformed parameter block for {module_name}")
    end = _balanced_end(clean, at, "(", ")")
    pending: list[tuple[str, str]] = []
    for fragment in _split_top_level(clean[at + 1 : end]):
        if "=" not in fragment:
            raise ShellPortError(f"parameter has no default in {module_name}: {fragment.strip()!r}")
        left, expression = fragment.split("=", 1)
        identifiers = _IDENT_RE.findall(left)
        if not identifiers:
            raise ShellPortError(f"parameter has no name in {module_name}: {fragment.strip()!r}")
        pending.append((identifiers[-1], expression.strip()))
    while pending:
        next_pending: list[tuple[str, str]] = []
        progress = False
        for name, expression in pending:
            try:
                value = eval_int(expression, values)
            except ShellPortError:
                next_pending.append((name, expression))
                continue
            values[name] = value
            progress = True
        if not progress:
            unresolved = ", ".join(f"{name}={expr}" for name, expr in next_pending)
            raise ShellPortError(f"unresolved parameter defaults in {module_name}: {unresolved}")
        pending = next_pending
    return values


def parse_module_declaration(
    source: str,
    module_name: str,
    *,
    constants: Mapping[str, int] | None = None,
    type_widths: Mapping[str, int] | None = None,
    type_signedness: Mapping[str, bool] | None = None,
) -> ModuleDeclaration:
    """Parse one ANSI module declaration into a width-resolved port inventory."""
    resolved_constants = discover_module_parameters(
        source, module_name, constants=constants
    )
    constants = resolved_constants
    type_widths = type_widths or {}
    type_signedness = type_signedness or {}
    decl_start, decl_end, ports_start, ports_end = _module_header_span(source, module_name)
    clean = strip_comments(source)
    header = clean[ports_start:ports_end]
    fragments = _split_top_level(header)
    ports: list[Port] = []
    inherited_type: str | None = None
    inherited_packed: tuple[str, ...] = ()
    inherited_direction: str | None = None

    for fragment in fragments:
        raw = fragment.strip()
        if not raw:
            continue
        direction_match = _DIRECTION_RE.match(raw)
        if direction_match:
            direction = direction_match.group(1)
            remainder = raw[direction_match.end() :].strip()
            identifiers = _top_level_identifiers(remainder)
            if not identifiers:
                raise ShellPortError(f"port declaration has no name: {raw!r}")
            name, name_start, name_end = identifiers[-1]
            type_text = _normalise_sv(remainder[:name_start]) or "logic"
            suffix = remainder[name_end:].strip()
            packed_texts = _dimension_texts(remainder[:name_start])
            inherited_direction = direction
            inherited_type = type_text
            inherited_packed = packed_texts
        else:
            if inherited_direction is None or inherited_type is None:
                raise ShellPortError(f"port continuation has no declaration to inherit: {raw!r}")
            identifiers = _top_level_identifiers(raw)
            if len(identifiers) != 1:
                raise ShellPortError(f"ambiguous port continuation: {raw!r}")
            name, name_start, name_end = identifiers[0]
            direction = inherited_direction
            type_text = inherited_type
            suffix = raw[name_end:].strip()
            packed_texts = inherited_packed
        if any(existing.name == name for existing in ports):
            raise ShellPortError(f"duplicate port name {name!r}")
        unpacked_texts = _dimension_texts(suffix)
        residue = suffix
        for dim in unpacked_texts:
            residue = residue.replace(dim, "", 1)
        if residue.strip():
            raise ShellPortError(f"unsupported text after port {name!r}: {residue.strip()!r}")
        packed = tuple(_parse_dimension(dim, constants) for dim in packed_texts)
        unpacked = tuple(_parse_dimension(dim, constants) for dim in unpacked_texts)
        base = _base_type(type_text)
        if base in _BUILTIN_WIDTHS:
            element_width = _BUILTIN_WIDTHS[base]
        elif base in type_widths:
            element_width = int(type_widths[base])
        else:
            raise ShellPortError(f"unknown packed type {base!r} on port {name!r}")
        for dim in packed:
            element_width *= dim.size
        bit_width = element_width
        for dim in unpacked:
            bit_width *= dim.size
        type_words = set(_IDENT_RE.findall(type_text))
        if "unsigned" in type_words:
            signed = False
        elif "signed" in type_words:
            signed = True
        elif base in type_signedness:
            signed = bool(type_signedness[base])
        else:
            signed = base in {"byte", "shortint", "int", "integer", "longint"}
        ports.append(
            Port(
                ordinal=len(ports),
                name=name,
                direction=direction,
                type_text=type_text,
                signal_type_text=_signal_type(type_text),
                packed_dimensions=packed,
                unpacked_dimensions=unpacked,
                signed=signed,
                element_width=element_width,
                bit_width=bit_width,
            )
        )
    if not ports:
        raise ShellPortError(f"module {module_name!r} has no parsed ports")
    declaration_text = source[decl_start:decl_end]
    return ModuleDeclaration(
        module_name=module_name,
        declaration_text=declaration_text,
        declaration_sha256=sha256_text(declaration_text),
        ports=tuple(ports),
    )


def discover_type_widths(
    source: str,
    *,
    constants: Mapping[str, int] | None = None,
) -> dict[str, int]:
    """Resolve simple packed enum/struct typedef widths from package source.

    This intentionally supports only the synthesizable declaration forms used by
    shell boundary packages.  An unresolved field/type is omitted, so a caller
    requesting it still receives the normal ``unknown packed type`` failure.
    """
    clean = strip_comments(source)
    values = dict(constants or {})
    constant_rows: list[tuple[str, str]] = []
    for match in re.finditer(r"\b(?:localparam|parameter)\b([^;=]*)=([^;]+);", clean):
        identifiers = _IDENT_RE.findall(match.group(1))
        if identifiers:
            constant_rows.append((identifiers[-1], match.group(2).strip()))
    changed = True
    while changed:
        changed = False
        for name, expression in constant_rows:
            if name in values:
                continue
            try:
                values[name] = eval_int(expression, values)
            except ShellPortError:
                continue
            changed = True

    widths: dict[str, int] = {}
    enum_re = re.compile(
        r"\btypedef\s+enum\s+([^\{;]+)\{.*?\}\s*([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
        re.S,
    )
    for match in enum_re.finditer(clean):
        base_text = _normalise_sv(match.group(1))
        dimensions = _dimension_texts(base_text)
        base = _base_type(base_text)
        if base not in _BUILTIN_WIDTHS:
            continue
        width = _BUILTIN_WIDTHS[base]
        try:
            for dim in dimensions:
                width *= _parse_dimension(dim, values).size
        except ShellPortError:
            continue
        widths[match.group(2)] = width

    struct_rows = list(
        re.finditer(
            r"\btypedef\s+struct\s+packed(?:\s+(?:signed|unsigned))?\s*\{(.*?)\}\s*"
            r"([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
            clean,
            re.S,
        )
    )
    aliases = list(
        re.finditer(
            r"\btypedef\s+([A-Za-z_$][A-Za-z0-9_$]*)\s+"
            r"([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
            clean,
        )
    )
    changed = True
    while changed:
        changed = False
        for match in struct_rows:
            name = match.group(2)
            if name in widths:
                continue
            fields = [field.strip() for field in match.group(1).split(";") if field.strip()]
            synthetic = "module __packed_struct(" + ", input ".join(fields) + "); endmodule\n"
            synthetic = synthetic.replace("module __packed_struct(", "module __packed_struct(input ", 1)
            try:
                declaration = parse_module_declaration(
                    synthetic,
                    "__packed_struct",
                    constants=values,
                    type_widths=widths,
                )
            except ShellPortError:
                continue
            widths[name] = declaration.total_bits
            changed = True
        for match in aliases:
            source_name, alias = match.group(1), match.group(2)
            if alias not in widths and source_name in widths:
                widths[alias] = widths[source_name]
                changed = True
    return widths


def discover_type_signedness(source: str) -> dict[str, bool]:
    """Resolve signedness for the packed typedef forms accepted above."""
    clean = strip_comments(source)
    signedness: dict[str, bool] = {}
    enum_re = re.compile(
        r"\btypedef\s+enum\s+([^\{;]+)\{.*?\}\s*([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
        re.S,
    )
    for match in enum_re.finditer(clean):
        base_text = _normalise_sv(match.group(1))
        words = set(_IDENT_RE.findall(base_text))
        base = _base_type(base_text)
        signedness[match.group(2)] = (
            False
            if "unsigned" in words
            else True
            if "signed" in words
            else base in {"byte", "shortint", "int", "integer", "longint"}
        )
    struct_re = re.compile(
        r"\btypedef\s+struct\s+packed(?:\s+(signed|unsigned))?\s*\{.*?\}\s*"
        r"([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
        re.S,
    )
    for match in struct_re.finditer(clean):
        signedness[match.group(2)] = match.group(1) == "signed"
    aliases = list(
        re.finditer(
            r"\btypedef\s+([A-Za-z_$][A-Za-z0-9_$]*)\s+"
            r"([A-Za-z_$][A-Za-z0-9_$]*)\s*;",
            clean,
        )
    )
    changed = True
    while changed:
        changed = False
        for match in aliases:
            source_name, alias = match.group(1), match.group(2)
            if alias not in signedness and source_name in signedness:
                signedness[alias] = signedness[source_name]
                changed = True
    return signedness


def load_policy_text(text: str) -> ShellPolicy:
    """Load the policy.  The committed .yml uses the JSON subset of YAML 1.2."""
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ShellPortError(f"policy is not valid JSON-compatible YAML: {exc}") from exc
    if not isinstance(data, dict):
        raise ShellPortError("policy root must be an object")
    expected_keys = {"schema_version", "module", "traffic_profile", "ports"}
    unknown = set(data) - expected_keys
    missing = expected_keys - set(data)
    if missing or unknown:
        raise ShellPortError(
            f"policy root keys mismatch: missing={sorted(missing)}, extra={sorted(unknown)}"
        )
    if not isinstance(data["ports"], list):
        raise ShellPortError("policy ports must be a list in declaration order")
    rows: list[PolicyPort] = []
    for index, raw in enumerate(data["ports"]):
        if not isinstance(raw, dict):
            raise ShellPortError(f"policy port row {index} must be an object")
        allowed = {
            "ordinal",
            "name",
            "direction",
            "domain",
            "driver",
            "sink",
            "dynamic_mask",
            "constant_reason",
        }
        extra = set(raw) - allowed
        if extra:
            raise ShellPortError(f"policy port row {index} has unknown keys {sorted(extra)}")
        for key in ("ordinal", "name", "direction", "domain"):
            if key not in raw:
                raise ShellPortError(f"policy port row {index} is missing {key!r}")
        mask_raw = raw.get("dynamic_mask")
        if mask_raw is None:
            mask = None
        elif isinstance(mask_raw, int):
            mask = mask_raw
        elif isinstance(mask_raw, str) and re.fullmatch(r"0[xX][0-9a-fA-F_]+", mask_raw):
            mask = int(mask_raw.replace("_", ""), 16)
        else:
            raise ShellPortError(
                f"policy port {raw.get('name', index)!r} has invalid dynamic_mask {mask_raw!r}"
            )
        rows.append(
            PolicyPort(
                ordinal=int(raw["ordinal"]),
                name=str(raw["name"]),
                direction=str(raw["direction"]),
                domain=str(raw["domain"]),
                driver=str(raw["driver"]) if raw.get("driver") is not None else None,
                sink=str(raw["sink"]) if raw.get("sink") is not None else None,
                dynamic_mask=mask,
                constant_reason=(
                    str(raw["constant_reason"]) if raw.get("constant_reason") is not None else None
                ),
            )
        )
    return ShellPolicy(
        schema_version=int(data["schema_version"]),
        module=str(data["module"]),
        traffic_profile=str(data["traffic_profile"]),
        ports=tuple(rows),
        raw_sha256=sha256_text(text),
    )


def validate_policy(declaration: ModuleDeclaration, policy: ShellPolicy) -> None:
    """Require exact declaration-order, shape, direction, and classification parity."""
    errors: list[str] = []
    if policy.schema_version != 1:
        errors.append(f"unsupported policy schema_version {policy.schema_version}, expected 1")
    if policy.module != declaration.module_name:
        errors.append(
            f"policy module {policy.module!r} does not match declaration {declaration.module_name!r}"
        )
    decl_names = [port.name for port in declaration.ports]
    policy_names = [row.name for row in policy.ports]
    missing = [name for name in decl_names if name not in set(policy_names)]
    extra = [name for name in policy_names if name not in set(decl_names)]
    duplicates = sorted({name for name in policy_names if policy_names.count(name) > 1})
    if missing:
        errors.append("policy is missing shell ports: " + ", ".join(missing))
    if extra:
        errors.append("policy has unknown shell ports: " + ", ".join(extra))
    if duplicates:
        errors.append("policy has duplicate shell ports: " + ", ".join(duplicates))
    if not missing and not extra and not duplicates and policy_names != decl_names:
        for ordinal, (decl_name, policy_name) in enumerate(zip(decl_names, policy_names)):
            if decl_name != policy_name:
                errors.append(
                    f"policy declaration order diverges at ordinal {ordinal}: "
                    f"expected {decl_name!r}, got {policy_name!r}"
                )
                break
    by_name = {row.name: row for row in policy.ports}
    for port in declaration.ports:
        row = by_name.get(port.name)
        if row is None:
            continue
        if row.ordinal != port.ordinal:
            errors.append(
                f"policy port {port.name!r} ordinal {row.ordinal} != declaration {port.ordinal}"
            )
        if row.direction != port.direction:
            errors.append(
                f"policy port {port.name!r} direction {row.direction!r} != {port.direction!r}"
            )
        if row.domain not in _ALLOWED_DOMAINS:
            errors.append(f"policy port {port.name!r} has unclassified domain {row.domain!r}")
        if port.direction == "input":
            if row.driver not in _ALLOWED_DRIVERS:
                errors.append(f"input {port.name!r} has invalid driver {row.driver!r}")
            if row.sink is not None:
                errors.append(f"input {port.name!r} must not declare sink {row.sink!r}")
            if row.driver == "top_port":
                if port.name not in {"gpu_clk", "vid_clk", "audio_clk", "rst_n"}:
                    errors.append(f"only clocks/reset may use top_port driver: {port.name!r}")
                if row.domain != "external":
                    errors.append(f"top port {port.name!r} must use external domain")
                if row.dynamic_mask not in (None, 0):
                    errors.append(f"top port {port.name!r} must not declare a stimulus dynamic mask")
            else:
                if row.domain != "gpu":
                    errors.append(f"pseudo-input {port.name!r} must be explicitly gpu-domain")
                if row.dynamic_mask is None:
                    errors.append(f"pseudo-input {port.name!r} is missing dynamic_mask")
                elif row.dynamic_mask < 0 or row.dynamic_mask >= (1 << port.bit_width):
                    errors.append(
                        f"pseudo-input {port.name!r} dynamic_mask does not fit {port.bit_width} bits"
                    )
                if row.dynamic_mask != (1 << port.bit_width) - 1 and not row.constant_reason:
                    errors.append(
                        f"pseudo-input {port.name!r} has intentional constant bits without a reason"
                    )
        elif port.direction == "output":
            expected_sink = f"{row.domain}_capture"
            if row.driver is not None:
                errors.append(f"output {port.name!r} must not declare driver {row.driver!r}")
            if row.domain not in {"gpu", "video", "audio"}:
                errors.append(f"output {port.name!r} must use a producer clock domain")
            elif row.sink != expected_sink or row.sink not in _ALLOWED_SINKS:
                errors.append(
                    f"output {port.name!r} sink {row.sink!r} != explicit {expected_sink!r}"
                )
            if row.dynamic_mask is None:
                errors.append(f"output {port.name!r} is missing dynamic_mask")
            elif row.dynamic_mask < 0 or row.dynamic_mask >= (1 << port.bit_width):
                errors.append(
                    f"output {port.name!r} dynamic_mask does not fit {port.bit_width} bits"
                )
            if row.dynamic_mask != (1 << port.bit_width) - 1 and not row.constant_reason:
                errors.append(
                    f"output {port.name!r} has intentional constant bits without a reason"
                )
        else:
            errors.append(f"unsupported direction {port.direction!r} on {port.name!r}")
    if errors:
        raise ShellPortError("; ".join(errors))


def policy_by_name(policy: ShellPolicy) -> dict[str, PolicyPort]:
    return {row.name: row for row in policy.ports}


def domain_output_bits(declaration: ModuleDeclaration, policy: ShellPolicy) -> dict[str, int]:
    rows = policy_by_name(policy)
    totals = {"gpu": 0, "video": 0, "audio": 0}
    for port in declaration.ports:
        if port.direction == "output" and port.name in rows:
            totals[rows[port.name].domain] += port.bit_width
    return totals


def flatten_port_elements(port: Port) -> tuple[tuple[str, int, int], ...]:
    """Return (SV selection suffix, element offset, element width) in declared order."""
    if not port.unpacked_dimensions:
        return (("", 0, port.element_width),)
    result: list[tuple[str, int, int]] = []

    def walk(depth: int, suffix: str) -> None:
        if depth == len(port.unpacked_dimensions):
            result.append((suffix, len(result) * port.element_width, port.element_width))
            return
        for index in port.unpacked_dimensions[depth].indices:
            walk(depth + 1, suffix + f"[{index}]")

    walk(0, "")
    return tuple(result)


def assert_exact_port_sets(labelled_sets: Mapping[str, Iterable[str]]) -> None:
    """Set-equality helper whose diagnostic names omissions instead of counting."""
    items = list(labelled_sets.items())
    if not items:
        raise ShellPortError("no port sets supplied for comparison")
    reference_label, reference_values = items[0]
    reference = set(reference_values)
    errors: list[str] = []
    for label, values in items[1:]:
        candidate = set(values)
        missing = sorted(reference - candidate)
        extra = sorted(candidate - reference)
        if missing or extra:
            errors.append(
                f"{label} differs from {reference_label}: missing={missing}, extra={extra}"
            )
    if errors:
        raise ShellPortError("; ".join(errors))


__all__ = [
    "Dimension",
    "ModuleDeclaration",
    "PolicyPort",
    "Port",
    "ShellPolicy",
    "ShellPortError",
    "assert_exact_port_sets",
    "domain_output_bits",
    "discover_module_parameters",
    "discover_type_widths",
    "eval_int",
    "flatten_port_elements",
    "load_policy_text",
    "parse_module_declaration",
    "policy_by_name",
    "sha256_text",
    "strip_comments",
    "validate_policy",
]
