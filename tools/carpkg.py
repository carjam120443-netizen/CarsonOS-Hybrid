#!/usr/bin/env python3
"""CarsonOS package repository and ISO build utility."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGES = ROOT / "packages"
CONFIG = PACKAGES / "repository.json"
INDEX = PACKAGES / "index.json"
REPO_NAME = "carsonos-stable"


def fail(message):
    print(f"carpkg: error: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        fail(f"missing {path.relative_to(ROOT)}")
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read {path}: {exc}")


def load_config(package_dir=PACKAGES):
    config = load_json(package_dir / "repository.json")
    for key in ("repository", "git_url", "branch", "catalog", "iso_path"):
        if not isinstance(config.get(key), str) or not config[key]:
            fail(f"repository.json needs a non-empty {key!r}")
    if config["repository"] != REPO_NAME:
        fail(f"unexpected repository name: {config['repository']}")
    if config["catalog"] != "packages/index.json":
        fail("catalog must remain packages/index.json unless repository config is deliberately changed")
    return config


def validate_package_tree(package_dir=PACKAGES):
    index = load_json(package_dir / "index.json")
    if index.get("format_version") != 1:
        fail("unsupported package index format_version")
    if index.get("architecture") != "x86_64":
        fail("package index architecture must be x86_64")
    entries = index.get("packages")
    if not isinstance(entries, list):
        fail("index packages must be an array")
    seen = set()
    for item in entries:
        if not isinstance(item, dict):
            fail("each package entry must be an object")
        name = item.get("name")
        if not isinstance(name, str) or not re.fullmatch(r"[a-z0-9][a-z0-9+_.-]{0,63}", name):
            fail(f"invalid package name: {name!r}")
        if name in seen:
            fail(f"duplicate package name: {name}")
        seen.add(name)
        for key in ("version", "description"):
            if not isinstance(item.get(key), str) or not item[key]:
                fail(f"package {name}: missing string field {key}")
        payload = item.get("payload")
        if payload is not None:
            if not isinstance(payload, str) or Path(payload).is_absolute() or ".." in Path(payload).parts:
                fail(f"package {name}: unsafe payload path")
            if not (package_dir / payload).is_file():
                fail(f"package {name}: payload file does not exist: {payload}")
        digest = item.get("sha256")
        if digest is not None and (not isinstance(digest, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", digest)):
            fail(f"package {name}: sha256 must be 64 hexadecimal characters")
    return entries


def cmd_repo(_args):
    cfg = load_config()
    print(f"Repository : {cfg['repository']}")
    print(f"Git source : {cfg['git_url']}")
    print(f"Branch     : {cfg['branch']}")
    print(f"Catalog    : {cfg['catalog']}")
    print(f"ISO path   : {cfg['iso_path']}")


def cmd_list(_args):
    cfg = load_config()
    entries = validate_package_tree()
    print(f"{cfg['repository']} ({len(entries)} packages)")
    for item in entries:
        print(f"{item['name']} {item['version']} - {item['description']}")


def cmd_validate(_args):
    cfg = load_config()
    entries = validate_package_tree()
    print(f"OK: {cfg['repository']}; {len(entries)} package entries validated")


def cmd_update(_args):
    cfg = load_config()
    if shutil.which("git") is None:
        fail("git is required for repository updates")
    with tempfile.TemporaryDirectory(prefix="carpkg-update-") as temp_name:
        temp = Path(temp_name)
        archive = temp / "packages.tar"
        checkout = temp / "checkout"
        checkout.mkdir()
        subprocess.run(["git", "init", str(checkout)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["git", "-C", str(checkout), "remote", "add", "origin", cfg["git_url"]], check=True)
        subprocess.run(["git", "-C", str(checkout), "fetch", "--depth=1", "origin", cfg["branch"], check=True)
        subprocess.run(["git", "-C", str(checkout), "archive", "--format=tar", "-o", str(archive), "FETCH_HEAD", "packages"], check=True)
        extracted = temp / "extracted"
        extracted.mkdir()
        with tarfile.open(archive, "r:") as tar:
            for member in tar.getmembers():
                target = (extracted / member.name).resolve()
                if not target.is_relative_to(extracted.resolve()):
                    fail("repository archive contains an unsafe path")
            tar.extractall(extracted)
        downloaded = extracted / "packages"
        if not (downloaded / "repository.json").is_file():
            fail("remote repository archive did not contain packages/repository.json")
        validate_package_tree(downloaded)
        # Keep the locally selected remote/branch; the fetched catalog must not
        # silently replace the user's repository configuration.
        shutil.copy2(CONFIG, downloaded / "repository.json")
        backup = temp / "old-packages"
        os.replace(PACKAGES, backup)
        try:
            os.replace(downloaded, PACKAGES)
        except Exception:
            os.replace(backup, PACKAGES)
            raise
    print(f"Updated {REPO_NAME} package folder from {cfg['git_url']} ({cfg['branch']})")


def find_limine_dir(value):
    start = Path(value).expanduser()
    if not start.is_absolute():
        start = ROOT / start
    candidates = [start]
    if start.is_dir():
        candidates.extend(p.parent for p in start.rglob("limine-bios.sys"))
    for candidate in candidates:
        if all((candidate / name).is_file() for name in (
            "limine-bios.sys", "limine-bios-cd.bin",
            "limine-uefi-cd.bin", "BOOTX64.EFI", "limine"
        )):
            return candidate
    fail(f"could not find a complete Limine binary directory under {start}")


def cmd_build_iso(args):
    load_config()
    validate_package_tree()
    kernel = Path(args.kernel)
    if not kernel.is_absolute():
        kernel = ROOT / kernel
    if not kernel.is_file():
        fail(f"kernel ELF not found: {kernel}")
    limine_dir = find_limine_dir(args.limine_dir)
    if shutil.which("xorriso") is None:
        fail("xorriso is required to build the ISO")
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="carsonos-iso-") as temp_name:
        iso_root = Path(temp_name) / "iso_root"
        (iso_root / "boot" / "limine").mkdir(parents=True)
        (iso_root / "EFI" / "BOOT").mkdir(parents=True)
        shutil.copy2(kernel, iso_root / "boot" / "kernel.elf")
        shutil.copy2(ROOT / "boot" / "limine.conf", iso_root / "boot" / "limine" / "limine.conf")
        shutil.copy2(limine_dir / "limine-bios.sys", iso_root / "boot" / "limine")
        shutil.copy2(limine_dir / "limine-bios-cd.bin", iso_root / "boot" / "limine")
        shutil.copy2(limine_dir / "limine-uefi-cd.bin", iso_root / "boot" / "limine")
        shutil.copy2(limine_dir / "BOOTX64.EFI", iso_root / "EFI" / "BOOT")
        iso_destination = Path(load_config()["iso_path"])
        if iso_destination.is_absolute() or ".." in iso_destination.parts:
            fail("iso_path must stay inside the ISO root")
        shutil.copytree(PACKAGES, iso_root / iso_destination)
        output.unlink(missing_ok=True)
        command = [
            "xorriso", "-as", "mkisofs", "-R", "-r", "-J",
            "-b", "boot/limine/limine-bios-cd.bin",
            "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
            "--efi-boot", "boot/limine/limine-uefi-cd.bin",
            "-efi-boot-part", "--efi-boot-image", "--protective-msdos-label",
            str(iso_root), "-o", str(output)
        ]
        subprocess.run(command, check=True)
    subprocess.run([str(limine_dir / "limine"), "bios-install", str(output)], check=True)
    print(f"Built ISO: {output}")
    print("Embedded package catalog: /PACKAGES/index.json")


def main():
    parser = argparse.ArgumentParser(prog="carpkg", description="CarsonOS package repository tools")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("repo", help="show the configured package repository")
    p.set_defaults(func=cmd_repo)
    p = sub.add_parser("list", help="list packages in the local catalog")
    p.set_defaults(func=cmd_list)
    p = sub.add_parser("validate", help="validate package metadata and paths")
    p.set_defaults(func=cmd_validate)
    p = sub.add_parser("update", help="update only the packages/ folder from its Git repository")
    p.set_defaults(func=cmd_update)
    p = sub.add_parser("build-iso", help="build a bootable ISO with packages/ embedded")
    p.add_argument("--kernel", default="build/kernel.elf")
    p.add_argument("--limine-dir", default="limine")
    p.add_argument("--output", default="CarsonOS-Hybrid-x86_64.iso")
    p.set_defaults(func=cmd_build_iso)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
