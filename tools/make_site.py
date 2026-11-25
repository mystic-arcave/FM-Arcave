#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""FM-ARCAVE's site, ready to publish: the home page, the emulator, the installer, the Link hub and the package.

    tools/make_site.py VERSION [PACKAGE.fwsc] [OUT]      (after web/emu/build.sh; OUT defaults to build/site)

    /                 web/site (index.html, site.css, img/, save/: the SAVES page, machines/: the MYSTIC MACHINES page)
    /machines/        with build/machines (tools/mm_build.py): catalog.json and the machines' files
    /emu/             build/emu (fm-arcave.wasm, the page, worklet.js, panel.js)
    /install/         web/install, with the version and the package's identity written in
    /link/            web/link
    /lib/             the update protocol (fm1ota.js, fm1pkg.js)
    /firmware/        fm-arcave-VERSION.fwsc, versions.json, LICENSE

Without a package the installer is left out and the home page still stands (CI: the emulator goes up even when
the FM-1 toolchain could not be fetched). A package is refused unless it carries the standard update loader and one
of FM-ARCAVE's own identities, FM-1_96N (from 0.10: FM-1_970 on).
"""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOADER_MARK = b"FELUCCA-LOADER-1"


def product_of(fwsc: bytes) -> str:
    """the identity: one marker byte in each of the first 20 blocks of 0x30 (fm1pkg.js productOf)"""
    return "".join(chr((fwsc[i * 0x30 + 0x2F] - i - 1) & 0xFF) for i in range(20) if fwsc[i * 0x30 + 0x2F] != 0x7D)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    ver = sys.argv[1]
    pkg = Path(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else None
    out = Path(sys.argv[3]) if len(sys.argv) > 3 else ROOT / "build" / "site"
    emu = ROOT / "build" / "emu"
    if not (emu / "fm-arcave.wasm").exists():
        raise SystemExit("make_site: run web/emu/build.sh first (no build/emu/fm-arcave.wasm)")
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(ROOT / "web" / "site", out)
    shutil.copytree(emu, out / "emu")
    shutil.copytree(ROOT / "web" / "link", out / "link")
    shutil.copytree(ROOT / "web" / "lib", out / "lib")
    mm = ROOT / "build" / "machines"             # the loadable machines (tools/mm_build.py), beside their page
    if (mm / "catalog.json").exists():
        for f in [mm / "catalog.json", *sorted(mm.glob("*.mmach")), *sorted(mm.glob("*.minst"))]:
            shutil.copy(f, out / "machines" / f.name)
        print(f"site: {len(list(mm.glob('*.mmach')))} loadable machine(s), {len(list(mm.glob('*.minst')))} instrument(s) from build/machines")
    else:
        print("site: no build/machines (tools/mm_build.py): the MACHINES page offers the firmware's own three only")
    (out / "firmware").mkdir()
    shutil.copy(ROOT / "LICENSE", out / "firmware" / "LICENSE")
    (out / ".nojekyll").write_text("")
    versions = {"latest": {"version": ver, "package": None, "product": None}}
    if pkg:
        data = pkg.read_bytes()
        product = product_of(data)
        if not re.fullmatch(r"FM-1_9[6-8]\d", product):
            raise SystemExit(f"make_site: {pkg.name} says {product!r}, not one of FM-ARCAVE's identities (FM-1_96N to FM-1_989)")
        if LOADER_MARK not in data:
            raise SystemExit(f"make_site: {pkg.name} has no standard update loader ({LOADER_MARK.decode()})")
        name = f"fm-arcave-{ver}.fwsc"
        (out / "firmware" / name).write_bytes(data)
        versions["latest"] = {"version": ver, "package": name, "product": product, "bytes": len(data)}
        page = (ROOT / "web" / "install" / "index.html").read_text()
        meta = json.dumps({"version": ver, "product": product, "pkg": "../firmware/" + name})
        page, n = re.subn(r"/\*META\*/.*?/\*META\*/", lambda _: meta, page, count=1, flags=re.S)
        if n != 1:
            raise SystemExit("make_site: the installer page has no META block")
        (out / "install").mkdir()
        (out / "install" / "index.html").write_text(page)
        print(f"site: package {name}, {len(data)} B, identity {product}")
    else:
        (out / "install").mkdir()
        (out / "install" / "index.html").write_text(
            '<!DOCTYPE html><meta charset="utf-8"><title>FM-ARCAVE</title><link rel="stylesheet" href="../site.css">'
            '<div class="wrap"><p style="padding:48px 0">This build has no FM-1 package. <a href="../">Back</a></p></div>')
        print("site: no package: the installer is left out")
    (out / "firmware" / "versions.json").write_text(json.dumps(versions, indent=1) + "\n")
    print(f"site: {out} ({sum(1 for p in out.rglob('*') if p.is_file())} files), version {ver}")


if __name__ == "__main__":
    main()
