{
  description = "spec2c — declarative C skeleton generator + vehir_lib";

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
        version = "0.2.0";
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
          mkdir -p $out/bin $out/share/spec2c
          cp spec2c $out/bin/
          cp skeleton.json $out/share/spec2c/
          cp -r templates $out/share/spec2c/
          cp -r lib $out/share/spec2c/
          runHook postInstall
        '';
      };

      tools-context = pkgs.stdenv.mkDerivation {
        pname = "tools-context";
        version = "0.1.0";
        src = ./.;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            tools-context.c -o tools-context -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp tools-context $out/bin/
          runHook postInstall
        '';
      };

      spec2c-check = pkgs.stdenv.mkDerivation {
        pname = "spec2c-check";
        version = "0.3.0";
        src = ./.;
        buildInputs = [ pkgs.cjson pkgs.ast-grep ];
        nativeBuildInputs = [ pkgs.pkg-config pkgs.makeWrapper ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            spec2c-check.c -o spec2c-check -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/share/spec2c
          cp spec2c-check $out/bin/
          cp soul-patterns.json $out/share/spec2c/
          wrapProgram $out/bin/spec2c-check \
            --prefix PATH : ${pkgs.ast-grep}/bin
          runHook postInstall
        '';
      };

      default = self.packages.${system}.spec2c;
    });
  };
}
