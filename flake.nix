{
  description = "Bare-metal STM32F103 + SSD1306 hello world";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        # NoCC: skip the host (macOS) C compiler so CC/OBJCOPY/... don't point at it.
        devShells.default = pkgs.mkShellNoCC {
          packages = with pkgs; [
            gcc-arm-embedded # arm-none-eabi-gcc/gdb/objcopy/objdump/size
            gnumake
            openocd # SWD: probe, flash, GDB server on :3333
            tio # USB-UART bridge on the debugger (115200 8N1)
          ]
          # st-info/st-flash: optional, and nixpkgs doesn't build it on Darwin.
          ++ lib.optional (lib.meta.availableOn stdenv.hostPlatform stlink) stlink;
        };
      });
}
