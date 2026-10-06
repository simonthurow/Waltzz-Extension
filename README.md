# Waltzz Extension — Setup Guide

With the given assumption of having unpacked the zip and now having `Waltzz/` `in/` and `harnesses/`.
Now only the binaryen library is missing.

## 1. Prerequisites
`
```bash
sudo apt update
sudo apt install -y build-essential cmake git
```

## 2. Build Binaryen

The mutator was developed against Binaryen tag **version_117** build that
exact version to avoid API mismatches:

```bash
git clone --recursive --branch version_117 https://github.com/WebAssembly/binaryen.git binaryen
cd binaryen
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build build -j"$(nproc)"
sudo cmake --install build
sudo ldconfig
```

The mutators Makefile links `-lbinaryen` from `/usr/local/lib`, which is
`cmake --install` default prefix `sudo ldconfig` just refreshes the
linkers cache so it picks up the newly installed `libbinaryen.so`.

If you ever see `libbinaryen.so: cannot open shared object file`, run
`ldconfig -p | grep binaryen` if it's empty the install step above didnt
run or didnt run with `sudo`.

## 3. Build the custom mutator

```bash
cd Waltzz/custom_mutators/wasm
make -j"$(nproc)"
```

This produces `libwasmmutator.so`. All three extensions (Threads/Atomics,
Multi-Memory, Relaxed-SIMD) are compiled in by default.

## 4. Build the fuzzing harness (`harnesses/wasmtime`)

This is the target binary AFL++ actually runs. A small Rust program that
feeds each generated module into `wasmtime`. Its `Cargo.toml` uses the 2024
edition, which is newer than the `cargo` apt package on most distros, so
install Rust via `rustup` rather than `apt install cargo`:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"
```

Then build it:

```bash
cd harnesses/wasmtime
cargo build --release
```

The resulting binary is `harnesses/wasmtime/target/release/wasmtime`.

## 5. If AFL++ itself isn't already built

If `Waltzz/afl-fuzz` isnt present or wont execute (e.g. it was built on a
different CPU architecture than this machine):

```bash
cd Waltzz
make all -j"$(nproc)"
```

## 6. Running Waltzz

```bash
cd Waltzz
AFL_CUSTOM_MUTATOR_LIBRARY=$PWD/custom_mutators/wasm/libwasmmutator.so \
  ./afl-fuzz -i ../in -o ../out \
  -- ../harnesses/wasmtime/target/release/wasmtime
```

- `-i ../in` — the seed corpus
- `-o ../out` — where AFL++ writes queue/crashes/hangs
- `AFL_CUSTOM_MUTATOR_LIBRARY` — points AFL++ at `libwasmmutator.so` instead
  of its own default mutators
