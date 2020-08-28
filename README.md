### zzrtl expert guide

For those already familiar with the starter guide (below) who want bleeding-edge functionality, please check out [jaredemjohnson's repo](https://github.com/jaredemjohnson/zzrtl-audio). It makes editing sounds/music/text (and more!) easier.

### zzrtl starter guide

This guide assumes you already have `zzrtl` (available on www.z64.me) and a clean OoT debug rom. Though it is compatible with all OoT/MM roms, you must first decompress them, so we're using the OoT debug rom for this example because it is already decompressed.

Note to Windows users: Use [Notepad++](https://notepad-plus-plus.org/) with C syntax highlighting for the optimal editing experience.

Now here are all the steps for dumping and building OoT:

Create a new folder.

Put your OoT debug rom in it. Name it `oot-debug.z64`. All lowercase, including the extension. If you don't like this name, edit `oot_dump.rtl`.

Save `oot_dump.rtl` and `oot_build.rtl` into the folder as well.

Linux users: use command line magic to execute `oot_dump.rtl`

Windows users:
The easiest thing you can do is drag-and-drop `oot_dump.rtl` onto `zzrtl.exe`. The second easiest thing you can do, which is more convenient long-term, would be to right-click `oot_dump.rtl` and associate all `.rtl` files with `zzrtl.exe`. Then you can execute any `.rtl` file by double-clicking it.

Once `oot_dump.rtl` finishes executing, `project.zzrpl` and some folders containing resources will be created. Now you can run `oot_build.rtl` to build a new rom.

The resulting rom will be `build.z64`, and its compressed counterpart will be `build-yaz.z64`. The initial compression will take a minute or two because it builds a cache, but subsequent compressions are faster. Other `zzrtl`-compatible codecs can be found [on this repo](https://github.com/z64me/z64enc). If you wish to disable compression, edit `oot_build.rtl`.

The default build scripts should do everything most people require. If you need extra functionality, read the manual.
http://www.z64.me/tools/zzrtl/manual
