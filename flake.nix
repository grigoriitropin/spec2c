{
  description = "spec2c — JSON Spec → Vehir-pattern C Tool generator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    pkgsFor = system: nixpkgs.legacyPackages.${system};
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      cflags = [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ];
    in {
      spec2c = pkgs.stdenv.mkDerivation {
        pname = "spec2c";
        version = "0.1.0";
        src = ./.;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            spec2c.c -o spec2c -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp spec2c $out/bin/
          runHook postInstall
        '';
      };

      default = self.packages.${system}.spec2c;
    });
  };
}
