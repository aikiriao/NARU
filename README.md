![eclint](https://github.com/aikiriao/NARU/workflows/eclint/badge.svg?branch=main)
![C/C++ CI](https://github.com/aikiriao/NARU/workflows/C/C++%20CI/badge.svg?branch=main)

# NARU

Natural-gradient AutoRegressive Unlossy audio compressor

**THIS IS AN EXPERIMENTAL CODEC. CURRENTLY SUPPORTS 16-bit/sample WAV FILE ONLY.**

# How to build

## Requirement

* [CMake](https://cmake.org) >= 3.15

## Build NARU Codec

```bash
git clone https://github.com/aikiriao/NARU.git
cd NARU/tools/naru_codec
cmake -B build
cmake --build build
```

# Usage

## NARU Codec

### Encode

```bash
./naru -e INPUT.wav OUTPUT.nar
```

you can change compression mode by `-m` option.
Following example encoding in maximum compression (but slow) option.

```bash
./naru -e -m 4 INPUT.wav OUTPUT.nar
```

### Decode

```bash
./naru -d INPUT.nar OUTPUT.wav
```

# License

MIT
