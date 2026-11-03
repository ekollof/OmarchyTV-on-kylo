# Tab/settings leaf evidence

Interpretation: [TAB-LEAF](../../reports/TAB-LEAF-2026-11-03.md). base.txt and
division.txt are read-only Ghidra extracts with trailing log whitespace removed.
The three TabDialog bodies are in ../tab-dispatch-2026-11-03/source.txt. Content
interface+18 isShowing was already resolved in TAB-WRITEBACK. No TV access.

```sh
make -C openaurora check-tab-leaf
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_leaf build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained full-source-hash/fragment-CRC checked reference now additionally
requires238860 leaf comparisons. It executes the shipped signed division too.
Framework/lower actions are controlled stubs, with direct field writes observed
before the end callback or after return; it does not create a vendor UI object.
Host/full, sanitizer and ARM logs record the other tests. Initial test-only
mapping diagnostics remain ignored under workbench; the corrected reference
passes and no live TV failure is implied.
