"""Synthetic local GIO tests; no USB device or personal files are accessed."""
import importlib.util
import os
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import Mock, patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

spec = importlib.util.spec_from_file_location(
    "deploy_switch", Path(__file__).resolve().parents[1] / "tools/deploy_switch.py")
deploy_switch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy_switch)
Gio = deploy_switch.Gio


class DeploymentTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tomotool-deploy-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sd = self.root / "1: SD Card"
        self.folder = self.sd / "switch"
        self.folder.mkdir(parents=True)
        self.source = self.root / "TomoToolNXPlusBorealis.nro"
        self.data = b"\0" * 16 + b"NRO0" + b"synthetic build" * 20
        self.source.write_bytes(self.data)
        os.utime(self.source, (2000, 2000))
        self.target = self.folder / self.source.name
        self.directory = Gio.File.new_for_path(str(self.folder))
        self.storage = Gio.File.new_for_path(str(self.sd))
        self.real_validate_storage = deploy_switch.validate_storage
        validator = patch.object(deploy_switch, "validate_storage", side_effect=lambda storage:
            None if storage.equal(self.storage) else self.real_validate_storage(storage))
        validator.start()
        self.addCleanup(validator.stop)
        backups = patch.object(deploy_switch, "BACKUPS", self.root / "backups")
        backups.start()
        self.addCleanup(backups.stop)

    def old_target(self, data=b"old build", timestamp=1000):
        self.target.write_bytes(data)
        os.utime(self.target, (timestamp, timestamp))

    def test_missing_and_older_are_copied_and_verified(self):
        for exists in (False, True):
            with self.subTest(exists=exists):
                if exists:
                    self.old_target()
                self.assertEqual(deploy_switch.deploy(self.source, self.storage), "copied")
                self.assertEqual(self.target.read_bytes(), self.data)
                self.assertEqual(int(self.target.stat().st_mtime), 2000)

    def test_equal_and_newer_are_never_replaced(self):
        for timestamp in (2000, 3000):
            with self.subTest(timestamp=timestamp):
                self.old_target(timestamp=timestamp)
                self.assertEqual(deploy_switch.deploy(self.source, self.storage), "skipped")
                self.assertEqual(self.target.read_bytes(), b"old build")

    def test_identical_older_file_is_not_rewritten(self):
        self.old_target(self.data)
        self.assertEqual(deploy_switch.deploy(self.source, self.storage), "skipped")
        self.assertEqual(int(self.target.stat().st_mtime), 1000)

    def test_dry_run_does_not_create_or_replace(self):
        self.assertEqual(deploy_switch.deploy(self.source, self.storage, dry_run=True), "dry-run")
        self.assertFalse(self.target.exists())
        self.old_target()
        deploy_switch.deploy(self.source, self.storage, dry_run=True)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_invalid_build_does_not_replace_target(self):
        self.old_target()
        self.source.write_bytes(b"not an nro")
        with self.assertRaises(deploy_switch.DeployError):
            deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_missing_or_unsafe_directory_is_rejected(self):
        storage = Gio.File.new_for_path(str(self.sd))
        for relative in ("/switch", "switch/../Saves", "Saves", "switch/missing", "switch\\bad"):
            with self.subTest(relative=relative), self.assertRaises(deploy_switch.DeployError):
                deploy_switch.destination_directory(storage, relative)
        self.assertEqual(deploy_switch.destination_directory(storage, "switch").get_path(), str(self.folder))
        (self.folder / "link").symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(deploy_switch.DeployError):
            deploy_switch.destination_directory(storage, "switch/link")

    def test_target_symlink_is_rejected(self):
        other = self.root / "untouched"
        other.write_bytes(b"untouched")
        self.target.symlink_to(other)
        with self.assertRaises(deploy_switch.DeployError):
            deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(other.read_bytes(), b"untouched")

    def test_unknown_remote_time_is_rejected(self):
        self.old_target()
        original = deploy_switch.info
        metadata = Mock()
        metadata.get_is_symlink.return_value = False
        metadata.get_file_type.return_value = Gio.FileType.REGULAR
        metadata.has_attribute.return_value = False
        def query(file):
            return metadata if file.get_path() == str(self.target) else original(file)
        with patch.object(deploy_switch, "info", side_effect=query):
            with self.assertRaisesRegex(deploy_switch.DeployError, "modification time"):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_failed_readback_is_not_reported_as_success(self):
        with patch.object(deploy_switch, "digest", side_effect=[b"expected", b"wrong"]):
            with self.assertRaisesRegex(deploy_switch.DeployError, "may be incomplete"):
                deploy_switch.deploy(self.source, self.storage)

    def test_copy_failure_is_reported(self):
        original = Gio.File.copy
        calls = []
        def copying(file, *args):
            calls.append(file)
            if len(calls) == 2:
                raise deploy_switch.GLib.Error("disconnected")
            return original(file, *args)
        with patch.object(Gio.File, "copy", new=copying):
            with self.assertRaisesRegex(deploy_switch.DeployError, "may be incomplete"):
                deploy_switch.deploy(self.source, self.storage)
        self.assertFalse(self.target.exists())

    def test_unrelated_executable_is_refused(self):
        for name in ("DBI.nro", "hbmenu.nro", "payload.bin", "test.nro"):
            source = self.root / name
            source.write_bytes(self.data)
            with self.subTest(name=name), self.assertRaises(deploy_switch.DeployError):
                deploy_switch.deploy(source, self.storage)
            self.assertFalse((self.folder / name).exists())

    def test_sensitive_folders_are_refused(self):
        for name in ("atmosphere", "bootloader", "Nintendo", "emuMMC", "switch/DBI", "switch/JKSV", "switch/TomoToolNXPlus/../DBI"):
            with self.subTest(name=name), self.assertRaises(deploy_switch.DeployError):
                deploy_switch.deploy(self.source, self.storage, relative=name)
        self.assertFalse(self.target.exists())

    def test_device_subpath_cannot_bypass_storage_selection(self):
        monitor = Mock()
        monitor.get_mounts.return_value = []
        with self.assertRaisesRegex(deploy_switch.DeployError, "exactly match"):
            deploy_switch.find_storage(monitor, "mtp://device/7:%20Saves")

    def test_storage_uri_must_be_sd_card_on_mtp(self):
        for uri in ("file:///tmp/1:%20SD%20Card", "mtp://device/7:%20Saves", "mtp://device/NAND/1:%20SD%20Card", "mtp://device/1:%20SD%20Card?other"):
            with self.subTest(uri=uri), self.assertRaises(deploy_switch.DeployError):
                self.real_validate_storage(Gio.File.new_for_uri(uri))

    def test_existing_build_has_verified_recoverable_copy(self):
        self.old_target()
        deploy_switch.deploy(self.source, self.storage)
        copies = list((self.root / "backups").glob("*/" + self.source.name))
        self.assertEqual(len(copies), 1)
        self.assertEqual(copies[0].read_bytes(), b"old build")

    def test_backup_failure_aborts_before_overwrite(self):
        self.old_target()
        original = Gio.File.copy
        def copying(file, *args):
            if file.get_path() == str(self.target):
                raise deploy_switch.GLib.Error("backup failed")
            return original(file, *args)
        with patch.object(Gio.File, "copy", new=copying):
            with self.assertRaises(deploy_switch.GLib.Error):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_insufficient_space_aborts_before_overwrite(self):
        self.old_target()
        with patch.object(deploy_switch, "require_space", side_effect=deploy_switch.DeployError("No space")):
            with self.assertRaises(deploy_switch.DeployError):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_changed_target_aborts_before_overwrite(self):
        self.old_target()
        def changed(storage, size):
            self.target.write_bytes(b"externally changed")
        with patch.object(deploy_switch, "require_space", side_effect=changed):
            with self.assertRaisesRegex(deploy_switch.DeployError, "changed during preflight"):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"externally changed")

    def test_unavailable_or_low_free_space_is_rejected(self):
        storage = Mock()
        metadata = Mock()
        storage.query_filesystem_info.return_value = metadata
        for known, available in ((False, 0), (True, 10)):
            metadata.has_attribute.return_value = known
            metadata.get_attribute_uint64.return_value = available
            with self.subTest(known=known), self.assertRaises(deploy_switch.DeployError):
                deploy_switch.require_space(storage, 1024)

    def test_backup_verification_failure_aborts_before_overwrite(self):
        self.old_target()
        # Snapshot, initial remote comparison, backup, remote verification.
        with patch.object(deploy_switch, "digest", side_effect=[b"new", b"old", b"bad", b"old"]):
            with self.assertRaisesRegex(deploy_switch.DeployError, "recovery copy did not verify"):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_disconnected_mount_aborts_before_overwrite(self):
        self.old_target()
        with patch.object(deploy_switch, "validate_storage", side_effect=[None, deploy_switch.DeployError("disconnected")]):
            with self.assertRaisesRegex(deploy_switch.DeployError, "disconnected"):
                deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")

    def test_zero_timestamp_requires_explicit_authorization(self):
        self.old_target(timestamp=0)
        with self.assertRaisesRegex(deploy_switch.DeployError, "replace-undated"):
            deploy_switch.deploy(self.source, self.storage)
        self.assertEqual(self.target.read_bytes(), b"old build")
        self.assertEqual(deploy_switch.deploy(self.source, self.storage, replace_undated=True), "copied")

    def mount(self, root):
        proxy = Mock(wraps=Gio.File.new_for_path(str(root)))
        proxy.get_uri_scheme.return_value = "mtp"
        mount = Mock()
        mount.get_root.return_value = proxy
        return mount

    def test_discovery_requires_dbi_sd_storage_and_rejects_ambiguity(self):
        monitor = Mock()
        monitor.get_mounts.return_value = [self.mount(self.root)]
        self.assertEqual(deploy_switch.find_storage(monitor).get_path(), str(self.sd))
        (self.root / "phone").mkdir()
        (self.root / "phone" / "SD Card").mkdir()
        monitor.get_mounts.return_value = [self.mount(self.root / "phone")]
        self.assertIsNone(deploy_switch.find_storage(monitor))
        monitor.get_mounts.return_value = [self.mount(self.root), self.mount(self.root)]
        with self.assertRaisesRegex(deploy_switch.DeployError, "Multiple"):
            deploy_switch.find_storage(monitor)

    def test_discovery_does_not_select_other_dbi_storages(self):
        self.folder.rmdir()
        self.sd.rmdir()
        (self.root / "7: Saves").mkdir()
        (self.root / "5: SD Card install").mkdir()
        monitor = Mock()
        monitor.get_mounts.return_value = [self.mount(self.root)]
        self.assertIsNone(deploy_switch.find_storage(monitor))


if __name__ == "__main__":
    unittest.main()
