{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    fenix = {
      url = "github:nix-community/fenix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, fenix }:
    let
      systems = [ "x86_64-linux" ];
      forEach = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forEach (system:
        let
          pkgs = import nixpkgs { inherit system; };
          llvm = pkgs.llvmPackages;

          fenixPkgs = fenix.packages.${system};
          rustToolchain = fenixPkgs.combine [
            fenixPkgs.complete.rustc
            fenixPkgs.complete.cargo
            fenixPkgs.complete.rust-src
            fenixPkgs.complete.clippy
            fenixPkgs.complete.rustfmt
            fenixPkgs.targets.x86_64-pc-windows-msvc.latest.rust-std
          ];
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.vcpkg-tool
              pkgs.xwin
              llvm.clang-unwrapped
              pkgs.lld
              llvm.llvm
              pkgs.git
              pkgs.curl
              pkgs.zip
              pkgs.unzip
              pkgs.gnutar
              pkgs.zstd
              pkgs.gdb
              pkgs.powershell
              pkgs.wine64Packages.stable
              rustToolchain
            ];

            shellHook = ''
              export VCPKG_ROOT="$PWD/External/vcpkg"
              export VCPKG_DISABLE_METRICS=1
              export XWIN_SPLAT_DIR="$PWD/.xwin/splat"
              export INCLUDE="$XWIN_SPLAT_DIR/crt/include;$XWIN_SPLAT_DIR/sdk/include/ucrt;$XWIN_SPLAT_DIR/sdk/include/um;$XWIN_SPLAT_DIR/sdk/include/shared;$XWIN_SPLAT_DIR/sdk/include/winrt"
              export LIB="$XWIN_SPLAT_DIR/crt/lib/x86_64;$XWIN_SPLAT_DIR/sdk/lib/um/x86_64;$XWIN_SPLAT_DIR/sdk/lib/ucrt/x86_64"
              export AR_x86_64_pc_windows_msvc=llvm-lib
              export WINEPREFIX="$PWD/.wine"
              export WINEDEBUG=-all

              if [ ! -d "$WINEPREFIX" ]; then
                env -u NIX_CFLAGS_COMPILE -u NIX_LDFLAGS wineboot --init >/dev/null 2>&1 || true
              fi

              # Regenerate the vcpkg overlay ports
              bash cmake/scripts/sync-vcpkg-overlays.sh >/dev/null \
                || echo "WARNING: sync-vcpkg-overlays.sh failed" >&2
              # The vcpkg registry checkout pins the tool release it expects; the
              # nixpkgs vcpkg-tool lags behind and chokes on newer manifest schema
              # versions. Only borrow it when the versions line up, otherwise let
              # vcpkg bootstrap the release it asks for.
              wantTag=$(sed -n 's/^VCPKG_TOOL_RELEASE_TAG=//p' \
                "$VCPKG_ROOT/scripts/vcpkg-tool-metadata.txt" 2>/dev/null || true)
              haveTag=$("$VCPKG_ROOT/vcpkg" version 2>/dev/null \
                | sed -n 's/.*version \([0-9-]*\)-.*/\1/p' | head -n1 || true)
              if [ -n "$wantTag" ] && [ "$haveTag" != "$wantTag" ]; then
                nixTag=$(vcpkg version 2>/dev/null \
                  | sed -n 's/.*version \([0-9-]*\)-.*/\1/p' | head -n1 || true)
                if [ "$nixTag" = "$wantTag" ]; then
                  ln -sfn "$(command -v vcpkg)" "$VCPKG_ROOT/vcpkg"
                else
                  rm -f "$VCPKG_ROOT/vcpkg"
                  "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics >/dev/null \
                    || echo "WARNING: bootstrap-vcpkg.sh failed" >&2
                fi
              fi
            '';
          };
        });
    };
}
