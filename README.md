# Decision Tree MPC: Build and Speed Tests

This repository contains a 3-party MPC implementation for decision-tree protocols.

## Prerequisites

- Linux environment
- `cmake` (>= 3.10)
- C++ toolchain with OpenMP support
- `gmp`, `openssl`, and pthread runtime/dev packages
- CUDA toolkit (required by the FSS third-party project build)
- Clone this repository with submodules, or initialize submodules before building.

## Build

Use the provided script:

```bash
./setup_env_and_build.sh
```

What it does:
1. Installs EMP toolkit dependencies.
2. Initializes submodules (`git submodule update --init --recursive`).
3. Configures and builds `third_party/fss`.
4. Configures and builds this project in `build/`.

## Run speed tests

Run the two decision-tree speed tests with the multiparty runner:

```bash
./run_multiparty_test.sh tree_speed_test
./run_multiparty_test.sh tree_batch_speed_test
```

Each command launches 3 parties and prints the aggregated results to `party0.log`.
