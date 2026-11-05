# Lua 5.4.9

Embedded in `mod_lua.so` only. These are the Lua.org / PUC-Rio sources from the
`v5.4.9` tag (archive SHA256
`76def9b3b100e029bfbe37c985d869d0ed993a0cf1a157df282d6dcd9d6a4e15`), minus the
standalone interpreter (`lua.c`, `onelua.c`) and the internal test harness
(`ltests.c`, `ltests.h`). Each remaining file keeps its own MIT copyright
notice. The openaurora module opens `io` and `os` with device and process
helpers removed, and it does not open `debug`, `package`, or `coroutine`.
It loads source text only.
