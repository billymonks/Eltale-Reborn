# Building

You need a decompressed US Quest 64 ROM. The steps below are: clone, install the compiler, run the recompiler, then build.

In the game itself, use a normal ROM. The decompressed ROM is only for the recompiler.

## 1. Clone the Quest64Recomp Repository
This project makes use of submodules so you will need to clone the repository with the `--recurse-submodules` flag.

```bash
git clone --recurse-submodules
# if you forgot to clone with --recurse-submodules
# cd /path/to/cloned/repo && git submodule update --init --recursive
```

## 2. Install Dependencies

### Linux
For Linux the instructions for Ubuntu are provided, but you can find the equivalent packages for your preferred distro.

```bash
# For Ubuntu, simply run:
sudo apt-get install cmake ninja libsdl2-dev libgtk-3-dev lld llvm clang-15
```

### Windows
You will need to install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).
In the setup process you'll need to select the following options and tools for installation:
- Desktop development with C++
- C++ Clang Compiler for Windows
- C++ CMake tools for Windows

You also need `make`, which you can install with [Chocolatey](https://chocolatey.org/):
```bash
choco install make
```

## 3. Generating the C code

Now that you have the required files, you must build [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) and run it to generate the C code to be compiled. The building instructions can be found [here](https://github.com/Mr-Wiseguy/N64Recomp?tab=readme-ov-file#building). That will build the executables: `N64Recomp` and `RSPRecomp` which you should copy to the root of the Quest64Recomp repository.

After that, go back to the repository root, and run the following commands:
```bash
./N64Recomp us.rev0.toml
./RSPRecomp aspMain.toml
```

## 4. Building the project

On Windows, open the repository folder in Visual Studio and build from there.

If you prefer the command line or you're on a Unix platform you can build the project using CMake:

```bash
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release # or Debug if you want to debug
cmake --build build-cmake --target Quest64Recompiled -j$(nproc) --config Release # or Debug
```

## 5. Run it

The executable is `EltaleReborn-PC` (`EltaleReborn-PC.exe` on Windows). Visual Studio puts it in `out/build/x64-[Configuration]`. The CMake commands above put it in `build-cmake`. On Windows the build also stages a player folder at `dist/EltaleReborn-PC`.

Run it from the repository root, or copy `assets` next to the executable. The build copies `mods` next to the executable as well.
