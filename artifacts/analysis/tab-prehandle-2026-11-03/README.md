# Tab pre-handler evidence

Interpretation: [TAB-PREHANDLE](../../reports/TAB-PREHANDLE-2026-11-03.md).
keys-focus.txt in ../tab-navigation-2026-11-03 is the original exact source body;
descriptor-return.txt adds read-only descriptor/caller evidence. No TV access.

```sh
make -C openaurora check-tab-prehandle
node artifacts/analysis/tab-prehandle-2026-11-03/verify-descriptor.cjs
make -C openaurora TARGET=tv BUILD=build-tv-v3/tab-policy \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/tab-policy/tests/tab_prehandle build-tv-v3/tab-policy/tests/tab_policy_firmware
node artifacts/analysis/tab-navigation-2026-11-03/verify.cjs
```

The maintained private-mapping reference additionally requires65664 pre-handler
cases. It stubs framework/backends, compares normalized handled truth and omits
source-invalid missing-child trees. It is offline/qemu-only, not a TV deployment.
