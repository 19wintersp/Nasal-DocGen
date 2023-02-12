# Nasal-DocGen

**A documentation generator for Nasal code.**

Licensed under the GNU General Public License, version 2.

## Build instructions

These instructions, as with the build system, are designed for Unix systems. If
you are able to extend this system to other platforms, contributions are welcome
to improve support.

The following libraries must also be available through `pkg-config`:

* [`cmark` - CommonMark C implementation](https://github.com/commonmark/cmark)
* [cJSON - Ultralightweight JSON parser](https://github.com/DaveGamble/cJSON)
* [`mustach` - Mustache C implementation](https://gitlab.com/jobol/mustach)

### Cloning

Both this repository and [SimGear](https://github.com/FlightGear/simgear) must
be cloned locally.

```bash
git clone https://github.com/19wintersp/Nasal-DocGen nasal-docgen/
git clone https://github.com/FlightGear/simgear simgear/
```

### Building SimGear

On most systems, assuming the required dependencies are installed, building the
SimGear project is simple (albeit slow):

```bash
cd simgear/
cmake .
make
```

If this does not work, see the build instructions from the SimGear project. Do
not ask for help with this stage here.

### Setting up

Once SimGear is built, the relevant object files from Nasal must be copied to
this program. All of the Nasal files, with the exception of those related to C++
and Emesary, should be included.

```bash
cd ../nasal-docgen/
mkdir -p nasal/
cp ../simgear/simgear/CMakeFiles/SimGearCore.dir/nasal/*.c.o nasal/
```

Emesary support is not required, and complicates the build process significantly
whilst being replaced trivially with a pair of empty functions. C++ bindings are
entirely superfluous. Non-C object files nonetheless included in the `nasal`
directory are ignored by the build process.

### Building `nasal-docgen`

To build this program, run:

```bash
make
```

The built program is written to `nasal-docgen`. To install it globally, run:

```bash
sudo make install
```

## User documentation

<!-- todo -->
