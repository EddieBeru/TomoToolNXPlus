#!/usr/bin/python3
"""Deploy a project NRO through DBI MTP on Linux (KDE or GVfs)."""

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import sys
import tempfile
import time
from urllib.parse import unquote, urlsplit
import uuid

from switch_deploy_policy import BUILD_NAMES, DIRECTORIES, DeployError, validate_source

try:
    from gi.repository import Gio, GLib
except ImportError:
    sys.exit("Use /usr/bin/python3 with PyGObject/GIO and a KDE or GVfs MTP service.")

ROOT = Path(__file__).resolve().parents[1]
ATTRIBUTES = "standard::type,standard::is-symlink,standard::size,time::modified"
BACKUPS = ROOT / ".deploy-backups"


def info(file):
    try:
        return file.query_info(ATTRIBUTES, Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, None)
    except GLib.Error as error:
        if error.matches(Gio.io_error_quark(), Gio.IOErrorEnum.NOT_FOUND):
            return None
        raise


def require_directory(directory):
    metadata = info(directory)
    if (metadata is None or metadata.get_is_symlink()
            or metadata.get_file_type() != Gio.FileType.DIRECTORY):
        raise DeployError("The SD card destination folder must already exist and be a directory.")


def validate_storage(storage):
    uri = urlsplit(storage.get_uri())
    if (uri.scheme != "mtp" or not uri.netloc or uri.query or uri.fragment
            or unquote(uri.path) != "/1: SD Card"):
        raise DeployError("Destination must be DBI's SD Card directly beneath a mounted MTP root.")
    root = storage.get_parent()
    mounted = Gio.VolumeMonitor.get().get_mounts()
    if not any(mount.get_root().equal(root) for mount in mounted):
        raise DeployError("The selected MTP device is no longer mounted.")


def destination_directory(storage, relative):
    if relative not in DIRECTORIES:
        raise DeployError("--directory must be switch, switch/TomoToolNXPlus or switch/TomoToolNXPlusBorealis.")
    directory = storage
    require_directory(directory)
    for component in PurePosixPath(relative).parts:
        directory = directory.get_child(component)
        require_directory(directory)
    return directory


def require_space(storage, size):
    metadata = storage.query_filesystem_info("filesystem::free", None)
    if (not metadata.has_attribute("filesystem::free")
            or metadata.get_attribute_uint64("filesystem::free") < size + 1024 * 1024):
        raise DeployError("Cannot establish sufficient SD free space; deployment aborted.")


def digest(file):
    stream = file.read(None)
    result = hashlib.sha256()
    try:
        while True:
            chunk = stream.read_bytes(1024 * 1024, None).get_data()
            if not chunk:
                break
            result.update(chunk)
    finally:
        stream.close(None)
    return result.digest()


def deploy(source, storage, relative="switch", dry_run=False, replace_undated=False):
    validate_storage(storage)
    directory = destination_directory(storage, relative)
    source = validate_source(source)
    require_directory(directory)
    # Snapshot the build so a concurrent rebuild cannot change the bytes being sent.
    with tempfile.TemporaryDirectory(prefix="tomotool-deploy-") as temporary:
        snapshot = Gio.File.new_for_path(str(Path(temporary) / source.name))
        Gio.File.new_for_path(str(source)).copy(snapshot, Gio.FileCopyFlags.NONE, None, None, None)
        with open(snapshot.get_path(), "rb") as build:
            build.seek(0x10)
            if build.read(4) != b"NRO0":
                raise DeployError("The build does not have a valid NRO header.")
        built_at = info(snapshot).get_attribute_uint64("time::modified")
        target = directory.get_child(source.name)
        current = info(target)
        if current is not None:
            if current.get_is_symlink() or current.get_file_type() != Gio.FileType.REGULAR:
                raise DeployError("The destination exists but is not a regular file.")
            if (not current.has_attribute("time::modified") or current.get_attribute_uint64("time::modified") == 0) and not replace_undated:
                raise DeployError("DBI did not provide a usable modification time; explicitly use --replace-undated after review.")
            if current.get_attribute_uint64("time::modified") >= built_at:
                print("Skipped: the Switch file has the same or a newer modification time.")
                return "skipped"
        print(f"{'Would copy' if dry_run else 'Copying'} {source.name} -> {target.get_uri()}", flush=True)
        if dry_run:
            return "dry-run"
        expected = digest(snapshot)
        if current is not None and digest(target) == expected:
            print("Skipped: the Switch already contains the same build.")
            return "skipped"
        backup = None
        previous = None
        if current is not None:
            backup_folder = BACKUPS / uuid.uuid4().hex
            backup_folder.mkdir(parents=True)
            backup = Gio.File.new_for_path(str(backup_folder / source.name))
            target.copy(backup, Gio.FileCopyFlags.NONE, None, None, None)
            with open(backup.get_path(), "rb") as recovery:
                os.fsync(recovery.fileno())
            folder_fd = os.open(backup_folder, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(folder_fd)
            finally:
                os.close(folder_fd)
            previous = digest(backup)
            if previous != digest(target):
                raise DeployError("The recovery copy did not verify; destination left unchanged.")
            print(f"Verified recovery copy: {backup.get_path()}", flush=True)
        # Recheck the storage, paths, capacity and old bytes immediately before writing.
        validate_storage(storage)
        checked_directory = destination_directory(storage, relative)
        if not checked_directory.equal(directory):
            raise DeployError("Destination changed during preflight.")
        require_space(storage, info(snapshot).get_size())
        latest = info(target)
        if current is None:
            if latest is not None:
                raise DeployError("Destination appeared during preflight; refusing to overwrite.")
        elif (latest is None or latest.get_is_symlink()
              or latest.get_file_type() != Gio.FileType.REGULAR
              or latest.has_attribute("time::modified") != current.has_attribute("time::modified")
              or latest.get_attribute_uint64("time::modified") != current.get_attribute_uint64("time::modified")
              or digest(target) != previous):
            raise DeployError("Destination changed during preflight; refusing to overwrite.")
        # MTP overwrite is not atomic. Do not claim success until readback matches.
        try:
            snapshot.copy(target, Gio.FileCopyFlags.OVERWRITE if current is not None else Gio.FileCopyFlags.NONE, None, None, None)
            if digest(target) != expected:
                raise DeployError("The copied file failed SHA-256 readback verification.")
        except (GLib.Error, DeployError) as error:
            raise DeployError(
                "Transfer failed; the destination NRO may be incomplete. "
                "Reconnect DBI and recopy the local build manually before launching it. "
                f"Recovery copy: {backup.get_path() if backup else 'no previous file'}. Details: {error}"
            ) from error
        print("Deployed: SHA-256 readback verified.")
        return "copied"


def find_storage(monitor, device=None):
    roots = [mount.get_root() for mount in monitor.get_mounts()
             if mount.get_root().get_uri_scheme() == "mtp"]
    if device:
        selected = Gio.File.new_for_uri(device)
        roots = [root for root in roots if root.equal(selected)]
        if not roots:
            raise DeployError("--device must exactly match a currently mounted MTP root, without a subpath.")
    matches = []
    for root in roots:
        children = root.enumerate_children("standard::name,standard::type,standard::is-symlink",
                                          Gio.FileQueryInfoFlags.NOFOLLOW_SYMLINKS, None)
        try:
            while True:
                child = children.next_file(None)
                if child is None:
                    break
                if (child.get_name() == "1: SD Card"
                        and not child.get_is_symlink()
                        and child.get_file_type() == Gio.FileType.DIRECTORY):
                    matches.append(root.get_child(child.get_name()))
        finally:
            children.close(None)
    if len(matches) > 1:
        raise DeployError("Multiple DBI SD cards found. Select one with --device mtp://.../.")
    return matches[0] if matches else None


def wait_for_storage(seconds, device=None, backend="auto"):
    monitor = Gio.VolumeMonitor.get()
    deadline = time.monotonic() + seconds
    print("Waiting for DBI MTP SD Card (KDE/GVfs).", flush=True)
    while True:
        context = GLib.MainContext.default()
        while context.pending():
            context.iteration(False)
        if backend != "gvfs" and not (device and device.startswith("mtp://")):
            from kde_mtp import discover
            storage = discover(device)
            if storage is not None:
                return "kde", storage
        if backend != "kde" and not (device and device.startswith("/modules/")):
            storage = find_storage(monitor, device)
            if storage is not None:
                return "gvfs", storage
        if time.monotonic() >= deadline:
            raise DeployError("No mounted DBI SD Card found. Connect USB, start DBI MTP, and open it in Files.")
        time.sleep(min(2, max(0, deadline - time.monotonic())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "TomoToolNXPlusBorealis.nro")
    parser.add_argument("--directory", default="switch", choices=sorted(DIRECTORIES), help="Existing project folder (default: switch)")
    parser.add_argument("--backend", choices=("auto", "kde", "gvfs"), default="auto")
    parser.add_argument("--device", help="KDE /modules/kmtpd/deviceN or GVfs mtp:// root")
    parser.add_argument("--replace-undated", action="store_true",
                        help="Explicitly allow replacing this named NRO if DBI supplies no usable date")
    parser.add_argument("--wait", type=float, default=60, metavar="SECONDS")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if args.wait < 0 or not args.wait < float("inf"):
        parser.error("--wait must be finite and nonnegative")
    if args.device and not (args.device.startswith("mtp://") or args.device.startswith("/modules/kmtpd/device")):
        parser.error("--device must be a GVfs mtp:// root or a KDE device object path")
    if args.device and ((args.backend == "kde" and args.device.startswith("mtp://")) or
                        (args.backend == "gvfs" and args.device.startswith("/modules/"))):
        parser.error("--device does not match the selected backend")
    if not args.source.is_file():
        parser.error(f"Build not found: {args.source}. Run make -j4 borealis first.")
    try:
        validate_source(args.source)
        backend, storage = wait_for_storage(args.wait, args.device, args.backend)
        if backend == "kde":
            from kde_mtp import deploy as deploy_kde
            deploy_kde(args.source, storage, BACKUPS, args.directory, args.dry_run, args.replace_undated)
        else:
            deploy(args.source, storage, args.directory, args.dry_run, args.replace_undated)
    except (DeployError, GLib.Error, OSError) as error:
        print(f"Deployment error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit("Stopped. If interrupted during copying, recopy the NRO before launching it.")
