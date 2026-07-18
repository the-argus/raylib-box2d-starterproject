{
  description = "underhanders development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    nixpkgs,
    flake-utils,
    ...
  }: let
    supportedSystems = let
      inherit (flake-utils.lib) system;
    in [
      system.aarch64-linux
      system.aarch64-darwin
      system.x86_64-linux
    ];
  in
    flake-utils.lib.eachSystem supportedSystems (system: let
      pkgs = import nixpkgs {inherit system;};
    in {
      devShell =
        (pkgs.mkShell.override {stdenv = pkgs.stdenv;})
        {
          packages =
            (with pkgs; [
              clang-tools # to get wrapped variant of clang-scan-deps

              # this command tries to be smart about where its removing from since
              # it does rm -rf. other commands will just fail, eh
              (writeShellScriptBin "cleanall" ''
                set -euo pipefail
                root="$(${git}/bin/git rev-parse --show-toplevel)"
                rm -rf "$root/vendor/.packages"
                rm -rf "$root/build-dev"
                rm -rf "$root/build-dev-noreload"
                rm -rf "$root/build-release"
              '')

              (writeShellScriptBin "webbuild-deps" "cmake -DEMSCRIPTEN=ON -P ./vendor/package_manager.cmake")
              (writeShellScriptBin "webconfigure" "emcmake cmake --preset dev-noreload")
              (writeShellScriptBin "webbuild" "cmake --build build-dev-noreload --parallel")
              (writeShellScriptBin "webrun" "emrun --port 8000 build-dev-noreload/underhanders.html")

              (writeShellScriptBin "build-deps" "cmake -P ./vendor/package_manager.cmake")
              (writeShellScriptBin "configure" "cmake --preset dev")
              (writeShellScriptBin "build" "cmake --build build-dev --parallel")
              (writeShellScriptBin "run" "build && gdb build-dev/underhanders")
              (writeShellScriptBin "frun" "build && ./build-dev/underhanders")
              (writeShellScriptBin "rd" "build && renderdoccmd capture -d . -c ./capture ./build-dev/underhanders")
              cmake
              ninja
              pkg-config
              cmake-format
              # tracy
              # renderdoc

              emscripten
            ])
            ++ pkgs.lib.optionals (system != flake-utils.lib.system.aarch64-darwin) (with pkgs; [
              mold
              gdb
              valgrind

              glfw
              libGLU
              libx11
              libxrandr
            ]);

          hardeningDisable = ["all"];
          NIX_ENFORCE_NO_NATIVE = false;
        };

      formatter = pkgs.alejandra;
    });
}
