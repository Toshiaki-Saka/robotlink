"""
libloader.py — locate the planar 2-link arm library (absorbed `tlm` core) and
expose it to the tlm_core binding via ``TLM_CORE_LIB``.

Import before importing anything from ``bindings``.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Dict, List

_ROOT = Path(__file__).resolve().parents[2]  # .../robotlink

_EXAMPLES: Dict[str, "tuple[str, str]"] = {
    "planar_2link": ("tlm_core", "TLM_CORE_LIB"),
}


def _libfile_names(base: str) -> List[str]:
    if sys.platform.startswith("win"):
        return [f"{base}.dll"]
    if sys.platform == "darwin":
        return [f"lib{base}.dylib"]
    return [f"lib{base}.so"]


def _find_lib(example: str, base: str) -> Path | None:
    build = _ROOT / "examples" / example / "core" / "build"
    search = [build, build / "Release", build / "Debug", build / "RelWithDebInfo"]
    # robotlink's root build collects tlm_core.dll at the top binary dir.
    for rb in (_ROOT / "build", _ROOT / "build" / "lib"):
        search += [rb, rb / "Release", rb / "Debug", rb / "RelWithDebInfo"]
    newest: Path | None = None
    for d in search:
        for name in _libfile_names(base):
            p = d / name
            if p.is_file():
                if newest is None or p.stat().st_mtime > newest.stat().st_mtime:
                    newest = p
    return newest


def prime() -> Dict[str, Path]:
    found: Dict[str, Path] = {}
    for example, (base, env) in _EXAMPLES.items():
        if os.environ.get(env):
            found[example] = Path(os.environ[env])
            continue
        lib = _find_lib(example, base)
        if lib is not None:
            os.environ[env] = str(lib)
            found[example] = lib
    return found


RESOLVED = prime()
