# Bundled fonts

Both faces below are redistributable, and both are embedded (subset to `[0-9: ]`)
in the shipped `.pbw`. Attribution and licence terms therefore travel with the
watchface, not just with this repo.

| File | Family | Licence | Source |
|---|---|---|---|
| `Roboto-Bold.ttf`, `Roboto-Light.ttf` | Roboto | Apache License 2.0 | Copied from the Pebble SDK's bundled copy (`toolchain/moddable/contributed/conversationalAI/fonts`) |
| `Montserrat-Bold.ttf`, `Montserrat-Light.ttf` | Montserrat | SIL Open Font License 1.1 | Copied from a local install of the Google Fonts release |

## Licence texts

Both are present, as both licences require them to accompany redistribution
(OFL 1.1 for the font itself even when embedded; Apache 2.0 for a copy of the
licence plus retention of attribution notices):

| File | Covers | Fetched from |
|---|---|---|
| `OFL-Montserrat.txt` | Montserrat | `raw.githubusercontent.com/JulietaUla/Montserrat/master/OFL.txt` |
| `LICENSE-2.0-Roboto.txt` | Roboto | `www.apache.org/licenses/LICENSE-2.0.txt` |

Neither was reconstructed from memory. The OFL body was additionally checked
byte-for-byte against an unrelated local copy of OFL 1.1 (after normalising
CRLF) to confirm it is the standard text.

Note the Roboto copy is the plain Apache 2.0 licence from apache.org. Apache 2.0
also expects any upstream `NOTICE` file to be carried forward; the SDK's bundled
Roboto ships no `NOTICE`, so there is nothing further to include — but if you
ever re-source Roboto from a distribution that has one, add it here.

## Not bundled, deliberately

**Bitham** — the original clock font — is a Pebble *system* font compiled into
the firmware and is used via `fonts_get_system_font()`. It ships no TTF in the
SDK. It is understood to be Gotham (Hoefler & Co.), a commercial typeface; a
desktop licence does not cover embedding in a distributed application, so it is
not bundled here at any size. This is why Bitham is capped at the firmware's
42px and Montserrat exists as the closest redistributable stand-in.
