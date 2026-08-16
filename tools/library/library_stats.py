#!/usr/bin/env python3
"""library_stats.py - Analyse the effects library catalogue.

Reports:
  - Total effects by class (star, terrain, celestial, corona, lod)
  - Implementation status
  - Missing renders
  - Copy-check readiness
"""

import sys
from pathlib import Path
try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Install with: pip install pyyaml")
    sys.exit(1)

def load_catalogue(path="effects-library.yaml"):
    """Load and validate the library catalogue."""
    cat_path = Path(path)
    if not cat_path.exists():
        # Try relative to this script
        cat_path = Path(__file__).parent.parent.parent / path

    with open(cat_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)

def analyse_catalogue(cat):
    """Generate analysis statistics."""
    effects = cat.get('effects', [])

    by_class = {}
    implemented = 0
    missing_render = 0
    has_render = 0

    for eff in effects:
        cls = eff.get('class', 'unknown')
        by_class[cls] = by_class.get(cls, 0) + 1

        if eff.get('implemented', False):
            implemented += 1

        if eff.get('render_path'):
            has_render += 1
        else:
            missing_render += 1

    total = len(effects)
    coverage = (implemented / total * 100) if total > 0 else 0

    return {
        'total': total,
        'by_class': by_class,
        'implemented': implemented,
        'missing_render': missing_render,
        'has_render': has_render,
        'coverage': coverage,
    }

def print_report(stats):
    """Print human-readable report."""
    print(f"=== Zhaozhou Effects Library ===")
    print(f"Version: {stats.get('version', 'unknown')}")
    print(f"Last updated: {stats.get('last_updated', 'unknown')}")
    print()
    print(f"Total effects: {stats['total']}")
    print(f"Implemented: {stats['implemented']} ({stats['coverage']:.1f}%)")
    print(f"With renders: {stats['has_render']}")
    print(f"Missing renders: {stats['missing_render']}")
    print()
    print("By class:")
    for cls, count in sorted(stats['by_class'].items()):
        print(f"  {cls}: {count}")

def print_list(cat):
    """Print all effects with status."""
    effects = cat.get('effects', [])

    print(f"{'ID':<30} {'Class':<12} {'Name':<20} {'Status':<12} {'Render'}")
    print("-" * 85)

    for eff in effects:
        eid = eff.get('id', 'unknown')[:30]
        cls = eff.get('class', 'unknown')[:12]
        name = eff.get('short_name', eff.get('name', 'unknown'))[:20]
        status = "DONE" if eff.get('implemented', False) else "TODO"
        render = "✓" if eff.get('render_path') else "✗"

        print(f"{eid:<30} {cls:<12} {name:<20} {status:<12} {render}")

def print_missing(cat):
    """Print only unimplemented effects."""
    effects = cat.get('effects', [])

    print(f"{'ID':<30} {'Class':<12} {'Name'}")
    print("-" * 60)

    missing = [e for e in effects if not e.get('implemented', False)]
    for eff in missing:
        eid = eff.get('id', 'unknown')[:30]
        cls = eff.get('class', 'unknown')[:12]
        name = eff.get('short_name', eff.get('name', 'unknown'))
        print(f"{eid:<30} {cls:<12} {name}")

    print()
    print(f"Total missing: {len(missing)}")

def main(argv):
    catalogue_path = argv[1] if len(argv) > 1 else "effects-library.yaml"

    try:
        cat = load_catalogue(catalogue_path)
        stats = analyse_catalogue(cat)
        stats['version'] = cat.get('version', 'unknown')
        stats['last_updated'] = cat.get('last_updated', 'unknown')
    except Exception as e:
        print(f"ERROR loading catalogue: {e}")
        return 2

    mode = argv[2] if len(argv) > 2 else "report"

    if mode == "list":
        print_list(cat)
    elif mode == "missing":
        print_missing(cat)
    else:
        print_report(stats)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
