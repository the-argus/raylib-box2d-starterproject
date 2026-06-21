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
        pkgs.mkShell
        {
          packages =
            (with pkgs; [
              clang-tools # to get wrapped variant of clang-scan-deps

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
