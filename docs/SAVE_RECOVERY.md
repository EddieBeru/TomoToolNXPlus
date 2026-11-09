# Save and exit recovery

## Behavior

- Save/exit processes every dirty Mii, Player, and Map file, regardless of the
  active tab. All pending files pass structural preflight before any are written.
- Before the first attempt, HTTP and USB sharing stop and a new full backup is
  created under the selected user's backup folder. Files are read back against
  their sources. Copy, enumeration, marker, and thread-start errors block saving.
- Each serialized file is closed and committed before the next file. Pending
  external writes receive a final checked commit. This is not an atomic multi-file
  transaction, and earlier commits are not rolled back when a later step fails.
- Write/close/commit failures retain the in-memory saves, dirty flags, and undo
  state. The save stays mounted and no success screen or exit occurs.
- The failure prompt offers A to retry and B to return to the editor, with touch
  equivalents. Discard/exit is unavailable while a failed save is unresolved.
  HTTP and USB remain paused so pending events cannot overwrite retained edits.
- Retries reuse the successful recovery backup instead of copying partially
  written files over it. Failed/incomplete snapshots are excluded from the backup
  picker. Backup-name collisions never overwrite an existing snapshot. Snapshot
  markers stay on the SD card and are skipped when restoring game files.
- Backup rotation happens after a replacement completes. A failed save retains
  its recovery snapshot even if this temporarily exceeds the configured limit.

## Verification

`make -j2` and host checks with AddressSanitizer/UndefinedBehaviorSanitizer passed
on 2026-11-08. The build introduced no new warning messages. English and Spanish
recovery keys are present; other locales use the established English fallback.

Host tests use synthetic fixtures and injected failures; commands are in
[tests/README.md](../tests/README.md). They cover structural validation, preservation
of unrelated fields/version, short writes, close errors, full-device errors,
backup verification failures, all pending-file combinations, each failure stage,
and retries after partial commits. They do not establish game compatibility,
physical Switch journal behavior, or emulator UI behavior.

Emulator and physical Switch verification for this change remain pending:

1. Make a small Player edit and a small Mii edit. Switch to WebUI, press B, choose
   Save, and confirm both changes remain after reopening the user.
2. Repeat with a Map edit pending. Switch to WebUI and press B. Confirm a save
   prompt appears and the map edit survives saving and reopening.
3. Check both destinations: B returns to user selection after success; Plus
   exits after success. Canceling the original prompt stays in the editor.
4. Check the recovery UI against a disposable synthetic save when a backup fails:
   the app stays open, shows the cause, and B returns to the editor with edits
   intact. Restore the backup destination and retry with A.
5. Inspect English and Spanish labels, touch buttons, disabled input during backup,
   and that success appears only after completion.

Immediate writes elsewhere in texture/Mii/WebUI/generator workflows are outside
this save/exit fix and still need their own safety review. Existing backups without
completion markers remain available when their three core save files are present;
they are not automatically certified by the new snapshot verifier.
