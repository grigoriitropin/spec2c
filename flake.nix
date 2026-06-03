{
  description = "spec2c — declarative C skeleton generator + build-time enforcement";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    pkgsFor = system: nixpkgs.legacyPackages.${system};
    src = ./.;
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      cflags = [ "-O2" "-Wall" "-Wextra" "-Werror" "-std=c2x" "-D_POSIX_C_SOURCE=200809L"
                 "-fno-ident" "-frandom-seed=spec2c" "-Wl,--build-id=none" ];
      S = "src/compile-specifications-into-source-code";
      inc = [
        "-Isrc"
        "-I${S}"
        "-I${S}/shared-type-declarations-across-modules"
        "-I${S}/parse-legacy-specification-file-format"
        "-Isrc/support-code-for-compiled-output"
      ];
      enforce_inc = inc ++ [
        "-I${S}/enforce-structural-rules-for-code"
        "-I${S}/verify-conformance-against-soul-patterns"
      ];
      runtime_src = [
        "src/runtime-for-generated-ipm-code.c"
        "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c"
        "src/support-code-for-compiled-output/file-string-and-json-parsing.c"
        "src/support-code-for-compiled-output/hash-table-and-substitution-code.c"
            "src/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c"
        "src/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c"
        "src/support-code-for-compiled-output/buffer-output-and-command-launch.c"
      ];
    in {
      # ── Standalone enforcement checker ────────────────────────────
      s2c-enforce = pkgs.stdenv.mkDerivation {
        pname = "s2c-enforce";
        version = "0.3.0";
        inherit src;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} ${builtins.toString enforce_inc} \
            ${S}/enforce-structural-rules-for-code/verify-structural-source-code-rules.c \
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            ${builtins.toString runtime_src} \
            -o s2c-enforce -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp s2c-enforce $out/bin/
          runHook postInstall
        '';
      };

      # ── spec2c compiler (enforcement gate inline) ─────────────────
      spec2c = pkgs.stdenv.mkDerivation {
        pname = "spec2c";
        version = "0.3.0";
        inherit src;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild

          # Step 1: Build enforcement checker
          cc ${builtins.toString cflags} ${builtins.toString enforce_inc} \
            ${S}/enforce-structural-rules-for-code/verify-structural-source-code-rules.c \
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            ${builtins.toString runtime_src} \
            -o s2c_enforce -lcjson

          # Step 2: Run enforcement gate (exits 1 on violation → build fails)
          ./s2c_enforce ./src

          # Step 3: Build spec2c
          cc ${builtins.toString cflags} ${builtins.toString inc} \
            ${S}/parse-command-dispatch-into-pipeline.c \
            ${S}/compile-abstract-instructions-into-code.c \
            ${S}/generate-output-from-ipm-specification.c \
            ${S}/parse-legacy-specification-file-format/parse-old-format-specification-data.c \
            ${S}/codegen-instruction-handler-function-set/emit-variable-declaration-handler-function.c \
            ${builtins.toString runtime_src} \
            -o spec2c -lcjson

          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/share/spec2c
          cp spec2c $out/bin/
          cp skeleton.json $out/share/spec2c/
          cp -r templates $out/share/spec2c/
          runHook postInstall
        '';
      };

      # ── IPM enforcer (compiled from IPM spec by spec2c) ──────────
      ipm-enforce = pkgs.stdenv.mkDerivation {
        pname = "ipm-enforce";
        version = "0.1.0";
        src = ./.;
        buildInputs = [ self.packages.${system}.spec2c pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild

          # Step 1: compile IPM enforcer spec → C, strip trailing JSON
          mkdir -p $TMPDIR/build
          spec2c enforce-naming-rules-via-ffi.ipm | sed '/^{"ok"/d' > $TMPDIR/build/ipm_enforce_gen.c
          # Remove duplicate includes and non-existent headers
          sed -i '/"enforce-naming-rules-via-ffi.h"/d' $TMPDIR/build/ipm_enforce_gen.c
          # Fix extern const-correctness (all params + return type)
          sed -i 's/extern char \* check_name_following_soul_rules(char \*/extern const char * check_name_following_soul_rules(const char */' $TMPDIR/build/ipm_enforce_gen.c
          sed -i 's/, char \* name, char \* fp/, const char *name, const char *fp/' $TMPDIR/build/ipm_enforce_gen.c
          sed -i 's/char \*err =/const char *err =/' $TMPDIR/build/ipm_enforce_gen.c
          sed -i "s/int errors = 0;/int errors = 0;\n    (void)errors;/" $TMPDIR/build/ipm_enforce_gen.c
          # Add runtime header
          sed -i '1i#include "runtime-for-generated-ipm-code.h"' $TMPDIR/build/ipm_enforce_gen.c

          # Step 2: compile generated C with runtime
          cc ${builtins.toString cflags} \
            -Isrc \
            -I${S} \
            -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -I${S}/enforce-structural-rules-for-code \
            -I${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns \
            $TMPDIR/build/ipm_enforce_gen.c \
            -Isrc \
            -I${S} \
            -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -I${S}/enforce-structural-rules-for-code \
            -I${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns \
            $TMPDIR/build/ipm_enforce_gen.c \
            src/support-code-for-compiled-output/file-string-and-json-parsing.c \
            src/support-code-for-compiled-output/hash-table-and-substitution-code.c \
            src/support-code-for-compiled-output/buffer-output-and-command-launch.c \
            src/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            src/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c \
            -o ipm-enforce -lcjson

          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp ipm-enforce $out/bin/
          runHook postInstall
        '';
      };

      default = self.packages.${system}.spec2c;
    });
  };
}
# force rebuild
