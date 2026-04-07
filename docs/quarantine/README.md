# Quarantined non-code artifacts

This directory holds non-code artifacts that are not part of the build/test surface.

## Why this exists

The repository naming policy requires predictable, automation-friendly file names. A legacy spreadsheet previously lived at repository root with spaces and non-ASCII characters in its name, which violated the placement/naming discipline used by repair prompts and CI tooling.

To avoid deleting potentially useful historical material while restoring hygiene, the file was:

- relocated out of repository root, and
- renamed to `dados_climaticos_teste.xlsx` (ASCII, lower_snake style).

Do not add new root-level binary artifacts. Keep non-code supporting files in scoped folders with ASCII-safe names.
