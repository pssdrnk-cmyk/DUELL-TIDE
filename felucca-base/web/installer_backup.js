// SPDX-License-Identifier: GPL-3.0-only
// DUELL installer: the backup the page saves ("Save a backup file", and before an install or a return). No DOM (install/index.html applies it;
// test_installer_state.mjs runs it against editor/mockunit.js).
//
// A current DUELL / TIDE unit (INFO caps bit 5: the patches are records) lists 24 objects in the plain LIST and the patches in pages. fm1backup.js's
// captureBackup knows only a fixed list of 11 to 40 ids and stopped with "Incomplete archive manifest". Such a unit is read the way the editor's Backup
// page reads it (editor/duellbackup.js: the "duell-backup" file, restored on that page). An older unit still gives the "felucca-backup" file.
import { parseInfo, captureBackup, fileName, CAPS_PATCH_RECORDS } from "./editor/duellbackup.js";
import { captureBackup as captureListed } from "./fm1backup.js";

// request: (cmd, args) -> reply, as fm1backup.js's BackupConnection.request; info: the unit's INFO reply (an array of bytes)
// Returns { file, name, format } (file: the object to save as JSON; format: "duell-backup" or "felucca-backup").
export async function captureUnitBackup(request, info, onProgress = () => {}) {
  const unit = parseInfo(info);
  if ((unit.caps ?? 0) & CAPS_PATCH_RECORDS)
    return { file: await captureBackup(request, unit, onProgress), name: fileName("DUELL-backup"), format: "duell-backup" };
  return { file: await captureListed(request, unit.version, onProgress), name: `DUELL-backup-${new Date().toISOString().slice(0, 10)}.json`, format: "felucca-backup" };
}
