JamBoxFM isolated manual-saving trial 0.1.0-dev

Development only: hardware_validated=false, release_enabled=false. Automatic saving is off.
Install only through the separate recovery trial page from a running JamBox application.
Factory, rescue and loader-only starting states are not accepted by this trial.
Keep a fresh full-flash backup. Initial storage setup replaces legacy projects and USER sounds.
Factory sounds and protected calibration are retained. Save now requires stopped playback.
The stable source and normal installer are independent of this candidate.
SHA256SUMS and source/tool hashes record consistency; they are not digital signatures.
Build: bash tools/cloud/build_device.sh --with-recovery
Package: python3 tools/package_jam_recovery.py --no-build
