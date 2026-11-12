"""Synthetic KDE deployment workflow and FD transport checks; no USB access."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import kde_mtp
from switch_deploy_policy import DeployError


class FakeStorage:
    def __init__(self):
        self.data = b"old NRO"
        self.date = 1000
        self.uploads = []
        self.validations = 0
        self.downloads = 0
        self.space = 10 * 1024 * 1024
        self.failure = None

    def validate(self):
        self.validations += 1
        if self.failure == "disconnect" and self.validations > 1:
            raise DeployError("disconnected")

    def metadata(self, path):
        if path == "/switch":
            return (1, 0, 65537, "switch", 0, 0, "inode/directory")
        if path != "/switch/TomoToolNXPlusBorealis.nro":
            return (0, 0, 0, "", 0, 0, "")
        return (2 if self.data is not None else 0, 1, 65537,
                "TomoToolNXPlusBorealis.nro", len(self.data or b""), self.date, "")

    def free_space(self):
        if self.failure == "changed":
            self.data = b"changed externally"
        return self.space

    def transfer(self, local, remote, upload=False, replace=False):
        if upload:
            if self.failure == "upload":
                raise DeployError("upload failed")
            assert bool(self.data is not None) == replace
            self.uploads.append(remote)
            self.data = Path(local).read_bytes()
        else:
            self.downloads += 1
            if self.failure == "backup":
                raise DeployError("backup failed")
            data = self.data
            if self.failure == "backup-mismatch" and self.downloads == 2:
                data = b"wrong backup"
            if self.failure == "readback" and self.uploads:
                data = b"wrong readback"
            Path(local).write_bytes(data)


class WorkflowTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "TomoToolNXPlusBorealis.nro"
        self.data = b"\0" * 16 + b"NRO0" + b"synthetic RGBA viewer build" * 10
        self.source.write_bytes(self.data)
        os.utime(self.source, (2000, 2000))
        self.storage = FakeStorage()
        self.backups = self.root / "backups"

    def deploy(self, **options):
        return kde_mtp.deploy(self.source, self.storage, self.backups, **options)

    def test_replace_verified_old_executable(self):
        self.assertEqual(self.deploy(), "copied")
        self.assertEqual(self.storage.data, self.data)
        self.assertEqual(self.storage.uploads, ["/switch/TomoToolNXPlusBorealis.nro"])
        self.assertEqual(next(self.backups.glob("*/previous.nro")).read_bytes(), b"old NRO")

    def test_missing_file_is_created(self):
        self.storage.data = None
        self.assertEqual(self.deploy(), "copied")
        self.assertEqual(self.storage.data, self.data)

    def test_equal_or_newer_is_not_replaced_even_with_undated_flag(self):
        for date in (2000, 3000):
            self.storage.date = date
            self.assertEqual(self.deploy(replace_undated=True), "skipped")
        self.assertEqual(self.storage.uploads, [])

    def test_undated_requires_explicit_option(self):
        self.storage.date = 0
        with self.assertRaisesRegex(DeployError, "replace-undated"):
            self.deploy()
        self.assertEqual(self.storage.uploads, [])
        self.assertEqual(self.deploy(replace_undated=True), "copied")

    def test_identical_undated_file_is_skipped_without_override(self):
        self.storage.data, self.storage.date = self.data, 0
        self.assertEqual(self.deploy(), "skipped")
        self.assertEqual(self.storage.uploads, [])

    def test_dry_run_does_not_write_or_create_backups(self):
        self.assertEqual(self.deploy(dry_run=True), "dry-run")
        self.assertFalse(self.backups.exists())
        self.assertEqual(self.storage.downloads, 0)
        self.assertEqual(self.storage.uploads, [])

    def test_preflight_failures_leave_remote_untouched(self):
        for failure in ("backup", "backup-mismatch", "disconnect", "changed"):
            with self.subTest(failure=failure):
                self.storage = FakeStorage()
                self.storage.failure = failure
                with self.assertRaises(DeployError):
                    self.deploy()
                self.assertEqual(self.storage.uploads, [])

    def test_no_space_aborts(self):
        self.storage.space = 0
        with self.assertRaisesRegex(DeployError, "space"):
            self.deploy()
        self.assertEqual(self.storage.uploads, [])

    def test_upload_and_readback_failures_report_recovery(self):
        for failure in ("upload", "readback"):
            with self.subTest(failure=failure):
                self.storage = FakeStorage()
                self.storage.failure = failure
                with self.assertRaisesRegex(DeployError, "Recovery:"):
                    self.deploy()
                self.assertTrue(list(self.backups.glob("*/previous.nro")))

    def test_unsafe_path_and_filename_are_refused(self):
        for directory in ("atmosphere", "switch/DBI", "switch/../Nintendo"):
            with self.subTest(directory=directory), self.assertRaises(DeployError):
                self.deploy(directory=directory)
        other = self.root / "DBI.nro"
        other.write_bytes(self.data)
        with self.assertRaises(DeployError):
            kde_mtp.deploy(other, self.storage, self.backups)
        self.assertEqual(self.storage.uploads, [])

    def test_invalid_nro_is_refused(self):
        self.source.write_bytes(b"not an NRO")
        with self.assertRaises(DeployError):
            self.deploy()
        self.assertEqual(self.storage.uploads, [])


class FakeConnection:
    def __init__(self):
        self.calls = []
        self.callback = None
        self.reject = False

    def signal_subscribe(self, *args):
        self.callback = args[-1]
        return 1

    def signal_unsubscribe(self, subscription):
        self.callback = None

    def call_with_unix_fd_list_sync(self, owner, storage, interface, method,
                                    args, result_type, flags, timeout, fds, cancel):
        self.calls.append(method)
        if self.reject:
            return kde_mtp.GLib.Variant("(i)", (1,)), None
        if method == "getFileToFileDescriptor":
            fd = fds.get(args.unpack()[0])
            try:
                os.write(fd, b"live remote bytes")
            finally:
                os.close(fd)
        self.callback(None, owner, storage, interface, "copyFinished", kde_mtp.GLib.Variant("(i)", (0,)))
        return kde_mtp.GLib.Variant("(i)", (0,)), None


class TransportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.file = Path(self.temporary.name) / "copy.nro"
        self.storage = kde_mtp.Storage.__new__(kde_mtp.Storage)
        self.storage.connection = FakeConnection()
        self.storage.owner, self.storage.storage = ":1.100", "/modules/kmtpd/device0/storage0"
        self.storage.validate = lambda: None
        self.storage.metadata = lambda path: (1,)
        self.storage.call = lambda *args: (0,)
        self.remote = "/switch/TomoToolNXPlusBorealis.nro"

    def test_download_waits_for_completion_and_closes_descriptors(self):
        self.storage.transfer(self.file, self.remote)
        self.assertEqual(self.file.read_bytes(), b"live remote bytes")
        self.assertIsNone(self.storage.connection.callback)

    def test_upload_deletes_only_selected_target_after_opening_source(self):
        self.file.write_bytes(b"local NRO")
        deleted = []
        self.storage.call = lambda *args: (deleted.append(args[-1].unpack()[0]) or 0,)
        self.storage.transfer(self.file, self.remote, upload=True, replace=True)
        self.assertEqual(deleted, [self.remote])
        self.assertEqual(self.storage.connection.calls, ["sendFileFromFileDescriptor"])

    def test_failed_source_open_never_deletes(self):
        with patch.object(self.storage, "call") as method:
            with self.assertRaises(OSError):
                self.storage.transfer(self.file, self.remote, upload=True, replace=True)
            method.assert_not_called()

    def test_transport_refuses_system_paths_before_any_access(self):
        with patch.object(self.storage, "validate") as validate:
            with self.assertRaises(DeployError):
                self.storage.transfer(self.file, "/atmosphere/payload.bin", upload=True)
            validate.assert_not_called()

    def test_rejected_transfer_does_not_wait_or_claim_success(self):
        self.storage.connection.reject = True
        with self.assertRaisesRegex(DeployError, "rejected"):
            self.storage.transfer(self.file, self.remote)
        self.assertIsNone(self.storage.connection.callback)


class DiscoveryTest(unittest.TestCase):
    def connection(self, count=1, present=True, description="1: SD Card"):
        class Bus:
            def call_sync(self, owner, path, interface, method, args, *rest):
                variant = kde_mtp.GLib.Variant
                if method == "NameHasOwner":
                    return variant("(b)", (present,))
                if method == "GetNameOwner":
                    return variant("(s)", (":1.42",))
                if method == "listDevices":
                    return variant("(ao)", ([f"/modules/kmtpd/device{i}" for i in range(count)],))
                if method == "listStorages":
                    return variant("(ao)", ([path + "/storage0"],))
                name = args.unpack()[1]
                value = {"friendlyName": "DBI MTP Responder", "description": description,
                         "udi": "synthetic-device-" + path}[name]
                return variant("(v)", (variant("s", value),))
        return Bus()

    def test_existing_daemon_connection_is_selected_and_pinned(self):
        with patch.object(kde_mtp.Gio, "bus_get_sync", return_value=self.connection()):
            storage = kde_mtp.discover()
            self.assertEqual(storage.owner, ":1.42")
            self.assertEqual(storage.storage, "/modules/kmtpd/device0/storage0")
            storage.validate()

    def test_absent_service_or_non_sd_storage_is_not_selected(self):
        for bus in (self.connection(present=False), self.connection(description="7: Saves")):
            with patch.object(kde_mtp.Gio, "bus_get_sync", return_value=bus):
                self.assertIsNone(kde_mtp.discover())

    def test_multiple_devices_require_exact_device_selection(self):
        with patch.object(kde_mtp.Gio, "bus_get_sync", return_value=self.connection(count=2)):
            with self.assertRaisesRegex(DeployError, "Multiple"):
                kde_mtp.discover()
            self.assertEqual(kde_mtp.discover("/modules/kmtpd/device1").device,
                             "/modules/kmtpd/device1")
            self.assertIsNone(kde_mtp.discover("/modules/kmtpd/device1/storage0"))


if __name__ == "__main__":
    unittest.main()
