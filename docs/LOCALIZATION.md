# English and Hebrew

English is the default. Hebrew can be chosen at three points:

1. **Boot menu** of the live image: *ZionOS Live (Hebrew)* starts the whole
   session in `he_IL.UTF-8`, mirrored right-to-left.
2. **Installer**: Calamares' language list includes Hebrew; the choice
   becomes the installed system's locale.
3. **Afterwards**: the *English / עברית* buttons in ZionOS Welcome, or
   *ZionOS Settings → Language*. ZionOS applications switch immediately
   (they restart); the rest of Plasma follows at the next login, because
   Plasma reads its language at session start.

## What ZionOS configures

* UTF-8 everywhere; `en_US.UTF-8` and `he_IL.UTF-8` are generated in the
  image and kept on installed systems (`zionos-locale-setup`).
* Keyboard: layouts `us` and `il`, switched with **Alt+Shift**
  (`/etc/xdg/kxkbrc`; live boot parameters set the same for the console and
  login screen). The shortcut is configurable in *ZionOS Settings → Keyboard*
  (Alt+Shift, Meta+Space, Ctrl+Shift, Caps Lock, Alt+Space, none); further
  layouts and options stay available in KDE System Settings.
* Fonts with Hebrew coverage, all freely redistributable: Noto Sans/Serif
  Hebrew (`fonts-noto-core`, OFL-1.1), Culmus (`fonts-culmus`, GPL-2 with
  font exception), DejaVu, Liberation 2.
* Spelling: `hunspell-he`, `hunspell-en-us`. Hebrew language packs for
  Firefox ESR and LibreOffice are preinstalled.

## ZionOS applications

* All user-visible strings go through Qt's `tr()`; the Hebrew catalogue is
  `packages/zionos-apps/translations/zionos_he.ts` (195 strings, complete).
  The unit tests fail if any string is untranslated, if the catalogue is
  older than the sources, or if a Hebrew string would be laid out
  left-to-right.
* Layout direction follows the language (`QGuiApplication::setLayoutDirection`).
* Mixed-direction text:
  * Sentences that begin with a Latin name or a placeholder carry a leading
    U+200F so the paragraph stays right-to-left.
  * Things that must read left-to-right inside Hebrew text — paths, host
    names, model identifiers, version strings — are wrapped in Unicode
    isolates (U+2066 … U+2069).
  * In the assistant, each paragraph of a reply takes its direction from its
    own first strong character, so Hebrew and English paragraphs can
    alternate; code blocks and command cards are always left-to-right.
  * Fields for API keys and model names are forced left-to-right.
  * Text copied as a command has bidi control characters removed.
* `.desktop` entries have Hebrew names and descriptions.

## Verified

`tests/vm/test_hebrew.py` (see `docs/READINESS.md` for the latest result),
using real key events in a VM: Hebrew boot entry → Hebrew session; ZionOS
Welcome and Settings shown in Hebrew; typing Hebrew after Alt+Shift and
English after switching back; a Hebrew file name typed in the terminal;
select-all + copy of mixed text; a Hebrew assistant conversation with a
streamed mixed-direction reply; LibreOffice converting a document with a
Hebrew file name (`desktop_checks.py`).

Unit tests: translation loading and layout flip, base-direction detection,
isolates keeping a command intact inside an RTL sentence, per-paragraph
direction in rendered Markdown.

## Not translated / known gaps

ZionOS does not claim a completely Hebrew system.

* **Upstream applications** are translated to the extent their projects
  provide: KDE Plasma and KDE applications have substantial but incomplete
  Hebrew coverage; untranslated strings appear in English.
* **Calamares installer**: Hebrew translation comes from upstream; the
  ZionOS slideshow caption is English only.
* **Boot menu** (GRUB) and early boot/console messages are English only.
* **Kernel, systemd and most command-line tools** print English messages.
* **Provider error details** (text returned by OpenAI/Anthropic, keyring
  back-end messages) are shown as received, in English, after a translated
  explanation.
* **Documentation** in this repository and `/usr/share/doc/zionos-base` is
  English only.
* The assistant's built-in instructions to the model are English; the model
  is asked to answer in the user's language.
* A native-speaker review of the Hebrew wording has **not** been done; the
  translations were written during development.
* Hebrew *installation* (choosing Hebrew inside Calamares and installing)
  has not been run end to end; the automated install test uses English.
