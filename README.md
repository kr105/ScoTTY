# Welcome to the KiTTY introduction web site

<p style="text-align: center;">
All KiTTY documentation is available on the official website<br/>
https://www.9bis.com/kitty/
</p>

## What is KiTTY ?
KiTTY is a fork from version 0.76 of **PuTTY**, the best telnet / SSH client in the world.
KiTTY is only designed for the Microsoft(c) Windows(c) platform. For more information about the original software, or pre-compiled binaries on other systems, you can go to the [Simon Tatham PuTTY page](http://www.chiark.greenend.org.uk/~sgtatham/putty/ "PuTTY").

KiTTY has all the features from the original software, and adds many others as described below:

### The very first requested features
* Sessions filter
* Portability
* Shortcuts for pre-defined command
* The session launcher
* Automatic logon script
* Automatic logon script with the RuTTY patch
* URL hyperlinks

### Technical features
* Automatic password
* Automatic command
* Running a locally saved script on a remote session

### Graphical features
* An icon for each session
* Send to tray
* Transparency
* Protection against unfortunate keyboard input
* Roll-up
* Always visible
* Quick start of a duplicate session
* Enhanced Configuration Box

### Other features
* Automatic saving
* SSH Handler: Internet Explorer integration
* pscp.exe and WinSCP integration
* Binary compression
* Clipboard printing
* Cygwin and cmd.exe integration
* File association
* Other settings
* New command-line options

### Bonus
* A light chat server is hidden in KiTTY
* A hidden text editor is integrated into KiTTY

## Official download page

KiTTY is available at our main CDN: [Fosshub](https://www.fosshub.com/KiTTY.html).

## How to compile

ScoTTY cross-compiles from Linux or WSL to Windows x86_64. There is no MSYS
step and nothing to install on Windows.

    sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 meson ninja-build

    meson setup build --cross-file cross/x86_64-w64-mingw32.ini
    ninja -C build

The six executables land in `build/0.76b_My_PuTTY/windows/`: `kitty.exe`,
`klink.exe`, `kscp.exe`, `ksftp.exe`, `kageant.exe` and `kittygen.exe`.

External libraries are not vendored as binaries. `libjpeg-turbo` and the POSIX
regex implementation are fetched from source and pinned by SHA256 in
`subprojects/*.wrap`, so the whole dependency chain is auditable.

### Running the tests

Tests are Windows executables, so running them from Linux needs wine:

    sudo apt install wine
    meson setup build --cross-file cross/x86_64-w64-mingw32.ini \
                      --cross-file cross/wine.ini
    meson test -C build

Without `cross/wine.ini` the tests still get compiled, they just do not run.

### Reproducible builds

The build is bit-for-bit reproducible: the same commit always produces the same
binaries, regardless of build path, timezone, umask or user. Timestamps come
from the commit date via `SOURCE_DATE_EPOCH`, never from the clock.

To verify it yourself:

    tools/verify-reproducible.sh

That builds the project twice under deliberately different conditions and
compares the hashes. CI runs the same check on every push, and tagged releases
publish `SHA256SUMS` together with a signed SLSA provenance attestation. If a
published binary does not match what you build from the same tag, something is
wrong.

To pin the toolchain exactly as CI does, build inside the Debian image and apt
snapshot named at the top of `.github/workflows/build.yml`.

### Editor / language server setup

Meson writes `build/compile_commands.json`, which clangd, ccls and most IDEs
pick up automatically. Point your editor at the build directory.

Original website is [https://www.9bis.com/kitty/](https://www.9bis.com/kitty/ "KiTTY website").
