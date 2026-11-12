"""DBI transfers through KDE's existing MTP daemon, without a second USB client."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import uuid

from gi.repository import Gio, GLib
from switch_deploy_policy import DeployError, remote_path, validate_source

BUS = "org.kde.kiod6"
DEVICE_INTERFACE = "org.kde.kmtp.Device"
STORAGE_INTERFACE = "org.kde.kmtp.Storage"


class Storage:
    def __init__(self, connection, owner, device, storage):
        self.connection, self.owner = connection, owner
        self.device, self.storage = device, storage
        self.identity = self.property(device, DEVICE_INTERFACE, "udi")

    def call(self, path, interface, method, arguments=None):
        return self.connection.call_sync(self.owner, path, interface, method, arguments,
                                        None, Gio.DBusCallFlags.NONE, 30000, None).unpack()

    def property(self, path, interface, name):
        return self.call(path, "org.freedesktop.DBus.Properties", "Get",
                         GLib.Variant("(ss)", (interface, name)))[0]

    def validate(self):
        if (not self.identity or self.property(self.device, DEVICE_INTERFACE, "udi") != self.identity
                or self.property(self.device, DEVICE_INTERFACE, "friendlyName") != "DBI MTP Responder"
                or self.storage not in self.call(self.device, DEVICE_INTERFACE, "listStorages")[0]
                or self.property(self.storage, STORAGE_INTERFACE, "description") != "1: SD Card"):
            raise DeployError("The selected DBI SD card changed or disconnected.")

    def metadata(self, path):
        return self.call(self.storage, STORAGE_INTERFACE, "getFileMetadata",
                         GLib.Variant("(s)", (path,)))[0]

    def free_space(self):
        return self.property(self.storage, STORAGE_INTERFACE, "freeSpaceInBytes")

    def transfer(self, local, remote, upload=False, replace=False):
        # Enforce the mutation boundary at the transport as well as the workflow.
        if not remote.startswith("/") or remote.count("/") < 2:
            raise DeployError("Invalid remote executable path.")
        directory, name = remote[1:].rsplit("/", 1)
        if remote != remote_path(directory, name):
            raise DeployError("Invalid remote executable path.")
        self.validate()
        flags = os.O_RDONLY if upload else os.O_WRONLY | os.O_CREAT | os.O_EXCL
        fd = os.open(local, flags | os.O_NOFOLLOW, 0o600)
        descriptor_list = Gio.UnixFDList.new()
        descriptor_index = descriptor_list.append(fd)
        loop, results = GLib.MainLoop(), []

        def completed(*arguments):
            results.append(arguments[-1].unpack()[0])
            loop.quit()

        subscription = self.connection.signal_subscribe(
            self.owner, STORAGE_INTERFACE, "copyFinished", self.storage,
            None, Gio.DBusSignalFlags.NONE, completed)
        expired = []

        def timeout():
            expired.append(True)
            loop.quit()
            return False

        timer = None
        try:
            if upload:
                current = self.metadata(remote)
                if bool(current[0]) != replace:
                    raise DeployError("Remote presence changed before upload.")
                # This is the same non-atomic replacement used by KDE's MTP worker.
                if replace and self.call(self.storage, STORAGE_INTERFACE, "deleteObject",
                                         GLib.Variant("(s)", (remote,)))[0] != 0:
                    raise DeployError("Could not remove the authorized NRO; upload aborted.")
            method = "sendFileFromFileDescriptor" if upload else "getFileToFileDescriptor"
            reply, _ = self.connection.call_with_unix_fd_list_sync(
                self.owner, self.storage, STORAGE_INTERFACE, method,
                GLib.Variant("(hs)", (descriptor_index, remote)), GLib.VariantType.new("(i)"),
                Gio.DBusCallFlags.NONE, 30000, descriptor_list, None)
            if reply.unpack()[0] != 0:
                raise DeployError("MTP transfer was rejected.")
            timer = GLib.timeout_add_seconds(120, timeout)
            if not results:
                loop.run()
            if expired or results != [0]:
                raise DeployError("MTP transfer failed or timed out; no automatic retry was attempted.")
            if not upload:
                os.fsync(fd)
        finally:
            if timer is not None and not expired:
                GLib.source_remove(timer)
            self.connection.signal_unsubscribe(subscription)
            os.close(fd)


def discover(device=None):
    connection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    present = connection.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameHasOwner", GLib.Variant("(s)", (BUS,)),
        None, Gio.DBusCallFlags.NONE, 30000, None).unpack()[0]
    if not present:
        return None
    owner = connection.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "GetNameOwner", GLib.Variant("(s)", (BUS,)),
        None, Gio.DBusCallFlags.NONE, 30000, None).unpack()[0]
    try:
        devices = connection.call_sync(owner, "/modules/kmtpd", "org.kde.kmtp.Daemon",
            "listDevices", None, None, Gio.DBusCallFlags.NONE, 30000, None).unpack()[0]
    except GLib.Error as error:
        if error.matches(Gio.dbus_error_quark(), Gio.DBusError.UNKNOWN_OBJECT):
            return None
        raise
    matches = []
    for path in devices:
        if device and device != path:
            continue
        probe = Storage.__new__(Storage)
        probe.connection, probe.owner = connection, owner
        if probe.property(path, DEVICE_INTERFACE, "friendlyName") != "DBI MTP Responder":
            continue
        for storage in probe.call(path, DEVICE_INTERFACE, "listStorages")[0]:
            if probe.property(storage, STORAGE_INTERFACE, "description") == "1: SD Card":
                matches.append(Storage(connection, owner, path, storage))
    if len(matches) > 1:
        raise DeployError("Multiple KDE DBI SD cards found; select --device /modules/kmtpd/deviceN.")
    return matches[0] if matches else None


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def flush_directory(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def deploy(source, storage, backups, directory="switch", dry_run=False, replace_undated=False):
    source = validate_source(source)
    remote = remote_path(directory, source.name)
    storage.validate()
    parent = None
    ancestors = []
    for component in directory.split("/"):
        path = (parent[1] if parent else "") + "/" + component
        record = storage.metadata(path)
        if (not record[0] or record[3] != component or record[6] != "inode/directory"
                or (parent and (record[1] != parent[0][0] or record[2] != parent[0][2]))):
            raise DeployError("Destination folders are missing or inconsistent.")
        parent = record, path
        ancestors.append((path, record))
    current = storage.metadata(remote)
    exists = bool(current[0])
    if exists and (current[1] != parent[0][0] or current[2] != parent[0][2]
                   or current[3] != source.name or current[6] == "inode/directory"):
        raise DeployError("Existing target metadata is inconsistent.")
    source_stat = source.stat()
    with source.open("rb") as stream:
        stream.seek(0x10)
        if stream.read(4) != b"NRO0":
            raise DeployError("The build does not have NRO magic.")
    if exists and current[5] > 0 and current[5] >= int(source.stat().st_mtime):
        print("Skipped: remote executable has an equal or newer timestamp.")
        return "skipped"
    if dry_run:
        print(f"Would inspect/update {remote} through KDE; dated targets retain the newer-only rule.")
        if exists and current[5] <= 0:
            print("DBI reports no usable timestamp; replacement requires --replace-undated.")
        return "dry-run"
    folder = Path(backups) / uuid.uuid4().hex
    folder.mkdir(parents=True)
    snapshot, recovery, check = folder / source.name, folder / "previous.nro", folder / "check.nro"
    shutil.copy2(source, snapshot)
    with snapshot.open("rb") as stream:
        os.fsync(stream.fileno())
        stream.seek(0x10)
        if stream.read(4) != b"NRO0":
            raise DeployError("Build changed while preparing its snapshot.")
    expected = digest(snapshot)
    latest_source = source.stat()
    if ((source_stat.st_ino, source_stat.st_size, source_stat.st_mtime_ns) !=
            (latest_source.st_ino, latest_source.st_size, latest_source.st_mtime_ns)
            or digest(source) != expected):
        raise DeployError("Build changed during snapshot; retry after compilation finishes.")
    previous = None
    if exists:
        storage.transfer(recovery, remote)
        storage.transfer(check, remote)
        previous = digest(recovery)
        if recovery.stat().st_size != current[4] or previous != digest(check):
            raise DeployError("Recovery reads differ; destination left unchanged.")
        if previous == expected:
            print("Skipped: Switch already contains the same build.")
            return "skipped"
        if current[5] <= 0 and not replace_undated:
            raise DeployError("DBI reports no usable timestamp. Review the recovery copy at "
                              f"{recovery}; explicitly use --replace-undated to replace this named NRO.")
    manifest = folder / "manifest.json"
    manifest.write_text(json.dumps({"remote": remote, "previous_sha256": previous,
                                    "new_sha256": expected}, indent=2))
    with manifest.open("rb") as stream:
        os.fsync(stream.fileno())
    flush_directory(folder)
    flush_directory(folder.parent)
    storage.validate()
    if storage.free_space() < snapshot.stat().st_size + 1024 * 1024:
        raise DeployError("Insufficient verified SD free space.")
    if storage.metadata(remote) != current:
        raise DeployError("Target changed during preflight.")
    if exists:
        last_check = folder / "preflight.nro"
        storage.transfer(last_check, remote)
        if digest(last_check) != previous:
            raise DeployError("Target contents changed during preflight.")
    # Revalidate every ancestor and storage relationship immediately before deletion.
    storage.validate()
    for component_path, record in ancestors:
        if storage.metadata(component_path) != record:
            raise DeployError("Destination directory changed during preflight.")
    if storage.metadata(remote) != current:
        raise DeployError("Target changed immediately before transfer.")
    try:
        storage.transfer(snapshot, remote, upload=True, replace=exists)
        readback = folder / "readback.nro"
        storage.transfer(readback, remote)
        if readback.stat().st_size != snapshot.stat().st_size or digest(readback) != expected:
            raise DeployError("SHA-256 readback mismatch.")
    except (DeployError, GLib.Error, OSError) as error:
        raise DeployError(f"Transfer failed; do not launch the destination NRO. Recovery: {recovery if exists else 'no previous file'}. {error}") from error
    print(f"Deployed {remote}; direct MTP SHA-256 readback verified. Recovery: {folder}")
    return "copied"
