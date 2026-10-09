# Installing horde

Purpose: get horde from download to a loaded instance in the reader's DAW on macOS or Windows,
in the formats that ship in 1.0 (CLAP, VST3, AU on the Mac), and say plainly what is not in 1.0
(AAX, a standalone app, Windows on ARM, Linux, AUv3).

## System requirements

Operating systems, processors and the CPU a typical patch needs, stated against the min-spec core
the module budgets are judged on.

Status: PENDING — B432, B439

[[TAB:install.requirements]]

## Plugin formats

Which formats ship (CLAP first, VST3 and AU wrapped), which do not, and which one to prefer in
each DAW.

Status: READY-TO-WRITE — ADR-186, ADR-190

[[TAB:install.formats]]

## Installing on macOS

The signed, notarised installer, what it puts where, and the first scan.

Status: PENDING — B433, B398

### Running the installer

The installer's screens and the choices on them.

Status: PENDING — B433, B412

[[FIG:install.mac-installer]]

### Where the files go

Plugin bundles, the preset store root and the support folder, per format.

Status: PENDING — B398, ADR-186

[[TAB:install.mac-paths]]

## Installing on Windows

The Windows x64 installer, the WebView2 runtime the editor needs, and the first scan. Gated on a
confirmed GUI in a real Windows host before any Windows release.

Status: PENDING — B447, B432, B433

### Running the installer

The installer's screens, signing and SmartScreen behaviour.

Status: PENDING — B433, B447

[[FIG:install.win-installer]]

### Where the files go

Plugin files, the preset store root and the support folder on Windows.

Status: PENDING — B398, B432

[[TAB:install.win-paths]]

## Finding horde in your DAW

How horde appears in a plugin browser, and what to do when a DAW needs a rescan.

Status: PENDING — B398, B427

[[FIG:install.daw-browser]]

### Names and identities

The display name, the vendor, and the identifiers a DAW stores in a project, which never change
after 1.0.

Status: READY-TO-WRITE — ADR-186, B428

[[TAB:install.identities]]

### horde next to horde legacy

The two are separate products and can be installed side by side; old projects keep loading the
archived legacy build.

Status: READY-TO-WRITE — ADR-186, B255

## Licensing and activation

How horde is licensed, and any activation step.

Status: PENDING — B412, B433

## Updating and uninstalling

How updates arrive, what an update promises about saved projects, and how to remove horde.

Status: PENDING — B412, ADR-186

## Third-party notices

The open-source components inside horde and their licences.

Status: PENDING — B433

[[TAB:install.third-party-notices]]
