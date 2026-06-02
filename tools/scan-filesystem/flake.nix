{
  description = "scan-filesystem — fast directory tree walker, JSON lines output";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    pkgsFor = system: nixpkgs.legacyPackages.${system};
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
    in {
      default = pkgs.stdenv.mkDerivation {
        pname = "scan-filesystem";
        version = "0.1.0";
        src = ./.;
        buildPhase = ''
          cc -O2 -Wall -Werror -std=c2x src/scanner.c -o scan-filesystem
        '';
        installPhase = ''
          mkdir -p $out/bin
          cp scan-filesystem $out/bin/
        '';
      };
    });
  };
}
