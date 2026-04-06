# Initial-condition reader compatibility and assumptions

This document records the explicit schema and conversion assumptions used by `cosmosim::io::readGadgetArepoHdf5Ic`.

## Supported container and groups

- File format: HDF5 with `/Header` and `/PartType[0..5]` groups.
- Required `/Header` attributes:
  - `NumPart_ThisFile` (6 entries)
  - `Time` (scale factor, positive)
- Optional `/Header` attribute:
  - `MassTable` (6 entries, used when `Masses` is absent and fallback is enabled)

## Dataset aliases

Aliases are accepted to improve interoperability with common GADGET/AREPO derivatives:

- Coordinates: `Coordinates`, `Position`, `POS`
- Velocities: `Velocities`, `Velocity`, `VEL`
- Particle IDs: `ParticleIDs`, `ParticleID`, `ID`
- Masses: `Masses`, `Mass`

The exact alias selected is stored in `IcImportReport::present_aliases` for provenance and audits.

## Conversion assumptions (conservative and explicit)

- Imported coordinates are interpreted as comoving coordinates and then converted to target frame
  (`comoving` or `physical`) according to `config.units.coordinate_frame`.
- Imported base units are assumed to be `kpc`, `msun`, and `km_s`.
- Conversions use `core::makeUnitSystem`, `comovingToPhysicalLength`, and
  `physicalToComovingLength`.

If a producer uses non-standard unit metadata, pre-convert externally or extend the reader with
explicit unit extraction before ingestion.

## Missing-field behavior

- Missing required datasets raise an exception.
- Missing optional fields are recorded in `IcImportReport::missing_optional_fields`.
- Defaulted values are recorded in `IcImportReport::defaulted_fields`, including:
  - generated particle IDs
  - velocity zero-fill
  - mass fallback from `MassTable`

## Current limitations

- Gas thermodynamic fields are not yet ingested into `GasCellSidecar`; this is tracked in
  `IcImportReport::unsupported_fields`.
- This reader currently imports particle-centric IC payloads only.
