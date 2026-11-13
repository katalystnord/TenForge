#!/usr/bin/env python3
"""Every class and property TenForge's PMD JSON-LD uses must exist in the
released ontologies it names: PMDco 3.1.2 and TTO 3.0.0 (fetched at test
time, pinned by SHA-256). Units must be QUDT unit IRIs from the fixed list
checked by hand on 2026-11-13 (each resolved, HTTP 200; PMD's own example's
unit:MegaPa does not).

    check_pmd_iris.py <export.jsonld> <pmdco-full.ttl> <tto-full.ttl>
"""

import json
import sys

QUDT_CHECKED = {"GigaPA", "MegaPA", "MilliM", "MilliM2", "N", "PERCENT"}


def names(o, out):
    if isinstance(o, dict):
        for k, v in o.items():
            if ":" in k and not k.startswith("@"):
                out.add(k)
            if k == "@type":
                for t in v if isinstance(v, list) else [v]:
                    out.add(t)
            if k == "@id" and isinstance(v, str) and v.startswith("unit:"):
                out.add(v)
            names(v, out)
    elif isinstance(o, list):
        for x in o:
            names(x, out)


def main():
    doc = json.load(open(sys.argv[1]))
    onto = open(sys.argv[2]).read() + open(sys.argv[3]).read()
    ctx = doc["@context"]
    used = set()
    names(doc, used)
    bad = []
    for n in sorted(used):
        prefix, local = n.split(":", 1)
        if prefix in ("rdfs", "xsd"):
            continue
        if prefix == "unit":
            ok = local in QUDT_CHECKED
        else:
            ok = (ctx[prefix] + local) in onto or n in onto
        print(("ok      " if ok else "MISSING ") + n)
        if not ok:
            bad.append(n)
    if bad:
        print("not in PMDco 3.1.2 or TTO 3.0.0:", ", ".join(bad))
        return 1
    print(len(used), "names, all found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
