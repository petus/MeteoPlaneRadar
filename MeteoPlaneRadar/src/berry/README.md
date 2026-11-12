# Berry

The [Berry](https://github.com/berry-lang/berry) scripting language, MIT
licence (see `LICENSE`), as used by MeteoPlaneRadar's script cards
(`../../Script.h`).

- `src/` is Berry's `src/` at commit `9f6fbc9` (2026-11-11), unchanged, plus three
  files of this project: `berry_conf.h` (the configuration), `be_port.c`
  (output, and "no" to files and input) and `be_modtab.c` (Berry's default
  module table).
- `generate/` is made from those by `tools/berry_prebuild.sh`. Do not edit
  it by hand.
