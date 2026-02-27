from __future__ import annotations

import os
from pathlib import Path
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DATASET_DIRNAME = "Siemens testing data results on RTStruct"
RTSTRUCT_PATTERNS = ("*.RTSTRUCT*.dcm", "*.rtstruct*.dcm")


def _env_path(name: str) -> Path | None:
    value = os.environ.get(name)
    if not value:
        return None
    return Path(value).expanduser()


def project_root() -> Path:
    return PROJECT_ROOT


def _iter_search_roots(extra_roots: Iterable[Path | str] | None = None) -> list[Path]:
    roots: list[Path] = []
    for env_name in ("BIOBOT_CASE_ROOT", "BIOBOT_DATA_ROOT"):
        env_root = _env_path(env_name)
        if env_root is not None:
            roots.append(env_root)

    dataset_root = PROJECT_ROOT / DEFAULT_DATASET_DIRNAME
    if dataset_root.exists():
        roots.append(dataset_root)
    roots.append(PROJECT_ROOT)

    if extra_roots:
        for root in extra_roots:
            roots.append(Path(root).expanduser())

    deduped: list[Path] = []
    seen: set[str] = set()
    for root in roots:
        key = str(root)
        if key in seen:
            continue
        seen.add(key)
        deduped.append(root)
    return deduped


def find_first_rtstruct(extra_roots: Iterable[Path | str] | None = None) -> Path:
    env_rtstruct = _env_path("BIOBOT_RTSTRUCT_PATH")
    if env_rtstruct is not None:
        if not env_rtstruct.is_file():
            raise FileNotFoundError(f"BIOBOT_RTSTRUCT_PATH does not exist: {env_rtstruct}")
        return env_rtstruct

    for root in _iter_search_roots(extra_roots):
        if not root.exists():
            continue
        for pattern in RTSTRUCT_PATTERNS:
            match = next(root.rglob(pattern), None)
            if match is not None:
                return match

    raise FileNotFoundError(
        "No RTSTRUCT DICOM file found. Set BIOBOT_RTSTRUCT_PATH or place data under "
        f"{PROJECT_ROOT / DEFAULT_DATASET_DIRNAME}"
    )


def infer_case_root(rtstruct_path: Path | str | None = None) -> Path:
    env_case_root = _env_path("BIOBOT_CASE_ROOT")
    if env_case_root is not None:
        return env_case_root

    rtstruct = Path(rtstruct_path) if rtstruct_path is not None else find_first_rtstruct()
    if len(rtstruct.parents) >= 2:
        return rtstruct.parents[1]
    return rtstruct.parent


def infer_image_root(rtstruct_path: Path | str | None = None) -> Path:
    env_image_root = _env_path("BIOBOT_IMAGE_ROOT")
    if env_image_root is not None:
        return env_image_root
    return infer_case_root(rtstruct_path)


def output_dir(name: str, create: bool = True) -> Path:
    out = PROJECT_ROOT / name
    if create:
        out.mkdir(parents=True, exist_ok=True)
    return out
