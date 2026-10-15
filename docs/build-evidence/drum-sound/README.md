# Individual drum sound trial evidence

Compiled source: `4c04594dd00c29892b1df0ce8b639191d5815644` (use the full revision in `build-receipt.json`).
Firmware SHA256: `a88970d9e601aed4042e28302d9ada19357b4ac549859a479f8eee5fba04b351`.
Package size: 611255 bytes. Application: 579652 / 581564 bytes; RAM: 91448 /
98304; pool: 329932 / 344064. Required RAM/pool reserves remain intact.

`validation.json` lists executed checks and actual log tails. Dedicated native
UI/DSP checks cover all 32 factory synth kits, the panel/GM/recipe mapping,
neighbor isolation, factory copying/cancellation, recording/save ownership,
linked hats, meaningful Tune availability, actual rendered kick/snare/hat audio
and live voice retirement. The same 2609 checks pass ASan/UBSan. The separate
NOR integration test copies and edits all four banks through the production UI
helpers, writes one intentional checkpoint and restores their exact bytes after
boot; editing alone performs no NOR writes.

The six production AudioWorklet scenarios include individual sound edits and
explicit factory copying. Installer checks exercise simulated complete transfer
and mobile layout only. No physical FM-1 installation is performed by these
tests. Owner device acceptance remains pending.

`publication.json` describes the isolated website commit. Remote verification
compares every staged package file, all outside Git objects/modes and unchanged
main. Raw HTTP artifact checks and direct Pages availability are recorded
separately. The normal installer, browser instrument and saving preview are
protected. The new source and evidence stay on the development branch.
