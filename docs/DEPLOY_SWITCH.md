# Deploy builds through DBI MTP

Run as the logged-in desktop user on Linux, without `sudo`. The helper reuses
PyGObject/GIO (`/usr/bin/python3` 3.11+, `gi.repository.Gio`) and the desktop MTP service;
it installs nothing. KDE uses `org.kde.kiod6` directly. GVfs uses a mounted MTP
root and requires its MTP backend. Do not run competing transfer clients while
copying. KDE may retain its connection without any Dolphin windows being open.

1. Connect the Switch by USB and start **Run MTP responder** in DBI.
2. Let KDE detect DBI, or open it in Files to mount it through GVfs.
3. Run `make -j4 deploy-borealis` (SDL: `make -j4 deploy-sdl`).

The target builds first and waits up to 60 seconds for DBI's SD card. Ordinary
`make` and `make borealis` never deploy automatically. The default destination
is `/switch/TomoToolNXPlusBorealis.nro` (SDL: `/switch/TomoToolNXPlus.nro`).

```sh
# Preview without writing to the Switch:
/usr/bin/python3 tools/deploy_switch.py --dry-run --wait 0
# Deploy the existing Borealis build:
/usr/bin/python3 tools/deploy_switch.py
# DBI commonly reports timestamp 0. Explicitly authorize replacement of an undated NRO:
make -j4 deploy-borealis DEPLOY_ARGS='--replace-undated'
```

## Device selection and restricted destinations

`--backend auto` prefers an existing KDE DBI connection, otherwise checks GVfs.
`--backend kde` and `--backend gvfs` select one explicitly. No service is killed,
USB driver changed, or alternate write transport tried after a transfer failure.
The helper waits for a desktop connection; it does not mount devices itself.

KDE discovery selects the exact friendly name `DBI MTP Responder` and storage
`1: SD Card`. Several matches cause an error. Use
`--device /modules/kmtpd/deviceN` to select among discovered KDE devices.
It pins the daemon's unique D-Bus owner and checks the device identity again
before writing. GVfs requires an exact mounted MTP root (`--device 'mtp://ROOT/'`),
not a path beneath it. `gio mount -li` lists GVfs roots. A renamed/localized
storage label is unsupported; the helper fails rather than guessing.

Only these existing destination folders are allowed:

- `switch`
- `switch/TomoToolNXPlus`
- `switch/TomoToolNXPlusBorealis`

Use `--directory` to select one. Source filenames must be `TomoToolNXPlus.nro`
or `TomoToolNXPlusBorealis.nro`; source symlinks are refused. `--source` can select
one of these named builds. Other executables, NAND, bootloader, atmosphere,
Nintendo, emuMMC, DBI and save-storage paths cannot be selected for writes.
No directories are created on the Switch.

## Update decisions and recovery

A missing destination is copied. An existing file with a usable equal/newer
whole-second timestamp is skipped. Keep PC and Switch clocks reasonably aligned.
An older file with identical contents is skipped too.

Missing/zero timestamps never imply an older file. `--replace-undated` explicitly
allows replacing the named undated executable; it cannot override a usable
newer timestamp, path restrictions, recovery or readback checks. With KDE, an
identical undated build is skipped without the option. Without authorization,
a differing undated target aborts; KDE retains the downloaded recovery copy for
review. Dry runs never write to the Switch or create recovery folders.

Before replacement, the helper downloads and verifies the existing executable
and flushes a uniquely named recovery copy to `.deploy-backups/` on the PC. These
copies are gitignored, survive `make clean`, and are not automatically removed.
KDE performs two independent remote reads, plus another immediately before
replacement. The local build is snapshotted and its NRO magic is checked.
Storage, destination metadata and free space (build size plus 1 MiB) must pass
preflight; changed targets and unavailable metadata cause an error.

MTP replacement is not atomic. KDE's MTP API replaces a file by deleting that
one executable and uploading the new bytes, as its own MTP worker does. Source
open and verified backup precede deletion. Completion waits for `copyFinished`
and checks its result. The uploaded NRO is then downloaded directly over MTP;
size and SHA-256 must match before success is reported. The helper does not
claim rename/rollback support or automatically restore after failure. Its KDE
implementation follows the official
[KDE MTP storage API](https://invent.kde.org/network/kio-extras/-/blob/master/mtp/kiod_module/mtpstorage.cpp).

Keep USB connected through verification. A failure, timeout or interruption can
leave the target absent or incomplete. Do not launch it: reconnect DBI and copy
the verified PC recovery NRO (or local build) back through the file manager.
An incomplete upload may have a new timestamp, so a later timestamp-based run
can skip it. There is no general force option. Only the named executable is
updated; saves and settings are not modified.

## Verification and limits

```sh
/usr/bin/python3 tests/deploy_switch_test.py
/usr/bin/python3 tests/kde_mtp_test.py
```

The 24 GVfs host tests cover update decisions, path restrictions, symlinks,
recovery, failures and discovery using temporary synthetic files/local GIO.
The KDE tests cover its workflow and actual FD transport adapter using fake
storage/D-Bus endpoints, including backup, upload and readback failures. No
personal save fixtures are used.

On 2026-11-12, three real Borealis updates through KDE's API completed with
verified recovery copies and direct MTP SHA-256 readback. Those transfers used
the temporary helpers from which the repository workflow was derived. The user
confirmed the final UI fixes on Switch. The consolidated repository workflow
has passed host tests; a read-only desktop probe correctly reported no DBI
connection while it was unavailable. No new physical upload with the final
repository script, or physical GVfs upload, is claimed.
