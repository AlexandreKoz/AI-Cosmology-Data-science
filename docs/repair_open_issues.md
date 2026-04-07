# Repair open issues (P01–P19 freeze ledger)

_Date captured: 2026-04-07 (UTC)_

This is the frozen open-issues ledger for emergency stabilization. It records **current** status only; no repairs are applied in this step.

## Evidence-backed blockers

| ID | Status | Area | Exact file(s) | Command(s) | Symptom |
|---|---|---|---|---|---|
| P01 | Open | HDF5 feature path compile | `src/io/restart_checkpoint.cpp` | `cmake -S . -B build/hdf5-debug ... -DCOSMOSIM_ENABLE_HDF5=ON`; `cmake --build build/hdf5-debug` | Compile fails when HDF5 is enabled; aligned vectors are incompatible with `writeDataset1d/readDataset1d` signatures expecting plain `std::vector`. |
| P02 | Open | HDF5 write helper container contract | `src/io/restart_checkpoint.cpp` | `cmake --build build/hdf5-debug` | `writeDataset1d(..., const std::vector<T>&)` rejects `core::AlignedVector<T>` call sites. |
| P03 | Open | HDF5 read helper container contract | `src/io/restart_checkpoint.cpp` | `cmake --build build/hdf5-debug` | `readDataset1d<T>() -> std::vector<T>` cannot be assigned directly into `core::AlignedVector<T>` fields (`operator=` mismatch). |
| P04 | Open | IC thermodynamic ingest gap | `src/io/ic_reader.cpp` | `nl -ba src/io/ic_reader.cpp | sed -n '490,493p'` | Runtime report warns that `PartType0` thermodynamic fields are bypassed and hydro sidecars remain defaulted. |
| P05 | Open | Missing HDF5 preset | `CMakePresets.json` | `cat CMakePresets.json` | No dedicated HDF5 preset exists; feature path currently requires manual configure flags. |
| P06 | Open | Missing HDF5+FFTW preset | `CMakePresets.json` | `cat CMakePresets.json` | No dedicated HDF5+FFTW preset exists for combined feature-path validation. |
| P07 | Open | FFTW discovery/preset disconnect | `CMakeLists.txt`, `CMakePresets.json` | `nl -ba CMakeLists.txt | sed -n '55,67p'`; `cat CMakePresets.json` | CMake has FFTW discovery logic, but no preset exercises it in CI-like flow. |
| P08 | Open | Build docs missing | `docs/build_instructions.md` | `test -f docs/build_instructions.md` | File absent. |
| P09 | Open | Contributor guide missing | `CONTRIBUTING.md` | `test -f CONTRIBUTING.md` | File absent. |
| P10 | Open | File naming policy violation | `Dados climáticos_teste.xlsx`, `docs/developer_file_placement.md` | `find . -maxdepth 1 -type f | sort`; `nl -ba docs/developer_file_placement.md | sed -n '3,7p'` | Root-level filename does not meet `lower_snake` naming convention. |

## P11–P19 tracking state

No in-repository artifact currently maps prompts/IDs `P11` through `P19` to concrete technical gaps by ID label. A full-text scan found no `P01..P19` tags in-tree.

Commands used:

```bash
rg -n "P19|P18|P17|P16|P15|P14|P13|P12|P11|P10|P09|P08|P07|P06|P05|P04|P03|P02|P01" .
```

Result: no matches.

Therefore current factual status is:

- P11: Open (unmapped in-tree; needs authoritative mapping source)
- P12: Open (unmapped in-tree; needs authoritative mapping source)
- P13: Open (unmapped in-tree; needs authoritative mapping source)
- P14: Open (unmapped in-tree; needs authoritative mapping source)
- P15: Open (unmapped in-tree; needs authoritative mapping source)
- P16: Open (unmapped in-tree; needs authoritative mapping source)
- P17: Open (unmapped in-tree; needs authoritative mapping source)
- P18: Open (unmapped in-tree; needs authoritative mapping source)
- P19: Open (unmapped in-tree; needs authoritative mapping source)

## Non-blocking but relevant baseline evidence

- CPU-only path is green:
  - `cmake --preset cpu-only-debug` (pass)
  - `cmake --build --preset build-cpu-debug` (pass)
  - `ctest --preset test-cpu-debug` (34/34 pass)
- HDF5 dependency behavior is explicit:
  - Without HDF5 dev package: configure fails with `Could NOT find HDF5`.
  - With HDF5 dev package: configure succeeds and build reaches compile-time container mismatch errors above.
