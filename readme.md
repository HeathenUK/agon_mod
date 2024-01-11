This is a tracker for .MOD files targetting the [Agon Light](https://www.thebyteattic.com/p/agon.html) system, requiring version 2.4.0+ of the [Console8 VDP firmware](https://github.com/AgonConsole8/agon-vdp) (future versions of the base Quark VDP will likely inherit the necessary fixes in future). It is implemented as a MOSlet, which means its compiled binary (playmod.bin) is intended to be placed in /mos/ within the Agon's SD Card and after which it can be called by simply typing `playmod file.mod`.

The project currently only supports 4 channel .MOD files. It does not currently support 6 or 8 channel .MOD files, nor .XM, .IT, or .S3M.

## Building

agon_mod is intended to compile within the AgDev toolchain, an extension of the CEDev toolchain that brings llvm-backed C compililation to the ez80 processer. Follow the instructions on that repository to clone first CEDev and then AgDev, after which agon_mod should compile within the CEDev sandbox environment.
