# Evidence map and raw-byte checks

- surface-settings.txt: fresh ARM/decompile SVGSurface::drawText eedfe4 and
  SettingsDialog::createSettingsList d5692c.
- label-style.txt: setLabel d5242c/d52498, UISurfaceImpl::drawText115ffec and
  BitmapFont's zero-return b42468 alias. Setter forwards/localizes/copies; it
  does not itself add a fixed prefix or padding. This does not say whether a
  particular localized string contains leading whitespace/control markup.
- list-create.txt: SettingItemsListBox::makeRow d5072c and base create10bdfb0.
- parent-init.txt: SVGListBoxBase constructor10bd120 initializes cached insets/
  row fields to0; SettingsDialog::init d5761c invokes createSettingsList.

Offline PT_LOAD reads (first LOAD VA-file offset0x10000) checked:

| address/chain | value/use |
|---|---|
|11602b0 ->154f700|vertical translation bytes3,1,2,4|
|14dbee4|SettingsDialog primary vtable+38 ->1152579 zero-margin base getter|
|14e87dc|PictureSettingsDialog same slot ->1152579|
|153b86c|SVGDialog same slot ->1152579|
|13f9340+1c/+20|BitmapFont vptr slots both b42469, verified zero getter|

The table names primary address points AFTER the8-byte C++ vtable header.
These are file-backed reads of the unchanged firmware, not live object samples.
They do not establish all ancestor windows, current row coordinates or text.
