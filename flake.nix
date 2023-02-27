{
  description = "A very basic flake";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, flake-utils, nixpkgs }@input:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ]
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          devShell = with pkgs; mkShell {
            nativeBuildInputs = [
              meson
              pkg-config
              ninja
              weston
              wayland
              wayland-scanner
            ];
            buildInputs = [
              wlroots_0_16
              pixman
              wayland-protocols
              eudev
              libxkbcommon
            ];
            shellHook = ''
              echo "welcome to wlroots"
              # export QT_LOGGING_RULES=*.debug=true
            '';
          };
        });
}
