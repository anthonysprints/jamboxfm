JamBoxFM local software candidate 0.1.0-dev

This candidate manifest records software validation only; hardware_validated remains false.
It does not itself authorize an installation. A separately packaged trial.json can bind
these exact bytes to the experimental factory-V15 first-install workflow.
That experiment does not establish hardware reliability or interrupted-update recovery.
manifest.json identifies the firmware, source, build evidence and pinned return package.
The return Sloop package is identified by hash only and is not included.
Source/toolchain hashes are reproducibility records, not digital signatures.
Rebuild: bash tools/cloud/build_device.sh --with-updater
Package: python3 tools/package_jam_device.py --no-build
