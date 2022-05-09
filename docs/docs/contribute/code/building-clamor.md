---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Building Clamor

*Before you start, ensure you've [set up your development environment](../getting-started/#development-environment).*

This guide will outline the process to build [Clamor](https://github.com/fragcolor-xyz/clamor) from the sources.

## Windows

!!! note
    Fragcolor doesn't yet officially support building and developing Clamor on the Windows platform. Please use Linux or Mac for Clamor. You may use the Linux/Mac [instructions](#linuxmac) on Windows with [WSL](https://docs.microsoft.com/en-us/windows/wsl/) but your mileage may vary and results are not guaranteed. Clamor is based on [Substrate](https://substrate.io/) so you could also check out their [support for Windows](https://docs.substrate.io/v3/getting-started/windows-users/).

## Linux/Mac

### Requirements

Ensure Rust and its dependencies are [installed](../getting-started/#install-setup-rust) on your system.

### Update system packages

 We use `rust nightly` so run `rustup update` every week when building Clamor. This will update your Rust installation, tools, and dependencies.

### Build & run the project

Clone the Clamor repository and checkout the [default branch](https://github.com/fragcolor-xyz/clamor).

Start your terminal, navigate to the Clamor root folder, and run the script-1/script-2 and then script-3.

- Script-1: Install, build, and run Clamor locally

    ```
    RUST_LOG=bitswap=trace,pallet_protos::pallet=trace,pallet_frag::pallet=trace cargo run -- --dev --tmp --rpc-external --rpc-port 9933 --rpc-cors all --ws-external --enable-offchain-indexing 1 --rpc-methods=Unsafe --ipfs-server --storage-chain
    ```

- Script-2: Same as script-1, but run local Clamor instance with a [chain specification](https://docs.substrate.io/v3/runtime/chain-specs/)

    ```
    cargo run -- --chain=spec_raw.json --validator --rpc-external --rpc-port 9933 --rpc-cors all --ws-external --enable-offchain-indexing 1 --rpc-methods=Unsafe --ipfs-server --storage-chain -d <DATA PATH>
    ```

- Script-3: Load specified test-assets onto the locally running Clamor instance

    ```
    docker run --rm --user root --network host -v `pwd`:/data chainblocks/cbl cbl /data/chains/add-test-assets.edn
    ```


??? note "Test Clamor with Polkadot.js"
    You can also test if your local instance of the Clamor blockchain is up and running by connecting it to the [Polkadot.js App Explorer](https://polkadot.js.org/apps/#/explorer).
    
    Click the top-left Pokadot icon in the header. Expand the `Development` sub-menu (at the bottom of the list), click `Local Node`, and then click `Switch` at the top. The App Explorer will connect with your local node and show the blocks being produced.

--8<-- "includes/license.md"