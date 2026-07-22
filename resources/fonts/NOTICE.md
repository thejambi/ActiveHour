# Bundled fonts

Both faces below are redistributable, and both are embedded (subset to `[0-9: ]`)
in the shipped `.pbw`. Attribution and licence terms therefore travel with the
watchface, not just with this repo.

| File | Family | Licence | Source |
|---|---|---|---|
| `Roboto-Bold.ttf`, `Roboto-Light.ttf` | Roboto | Apache License 2.0 | Copied from the Pebble SDK's bundled copy (`toolchain/moddable/contributed/conversationalAI/fonts`) |
| `Montserrat-Bold.ttf`, `Montserrat-Light.ttf` | Montserrat | SIL Open Font License 1.1 | Copied from a local install of the Google Fonts release |

## Outstanding

**The full licence texts are not yet in this repo.** Both licences require them
to accompany redistribution:

- OFL 1.1 requires the copyright notice and licence to be distributed with the
  font (including when embedded).
- Apache 2.0 requires a copy of the licence and retention of attribution
  notices.

Add `OFL.txt` (Montserrat) and `LICENSE-2.0.txt` (Roboto) alongside these files,
taken from the upstream Google Fonts repositories, before publishing a release.
Neither was available offline on the machine these were copied from, so this was
left as a deliberate to-do rather than reconstructed from memory.

## Not bundled, deliberately

**Bitham** — the original clock font — is a Pebble *system* font compiled into
the firmware and is used via `fonts_get_system_font()`. It ships no TTF in the
SDK. It is understood to be Gotham (Hoefler & Co.), a commercial typeface; a
desktop licence does not cover embedding in a distributed application, so it is
not bundled here at any size. This is why Bitham is capped at the firmware's
42px and Montserrat exists as the closest redistributable stand-in.
