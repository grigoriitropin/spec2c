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
        "src/support-code-for-compiled-output/structural-rule-checker-batch-two/structural-rule-checker-batch-two.c"
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
      spec2c enforce-naming-rules-via-ffi.ipm > ipm_enforce_gen.c
          sed -i '/^{"ok"/d' ipm_enforce_gen.c
          sed -i '/"enforce-naming-rules-via-ffi.h"/d' ipm_enforce_gen.c
          sed -i '1i#include "runtime-for-generated-ipm-code.h"' ipm_enforce_gen.c
          sed -i 's/extern char \* check_name_following_soul_rules(char \*/extern const char * check_name_following_soul_rules(const char */' ipm_enforce_gen.c
          # Fix all extern const-correctness: char *path → const char *path
          sed -i 's/extern \(.*\)(char \* path)/extern \1(const char *path)/' ipm_enforce_gen.c
          sed -i 's/extern \(.*\)(char \* dirpath)/extern \1(const char *dirpath)/' ipm_enforce_gen.c
          sed -i 's/extern char \* check_name_following_soul_rules(char \*/extern const char * check_name_following_soul_rules(const char */' ipm_enforce_gen.c
          sed -i 's/, char \* name, char \* fp/, const char *name, const char *fp/' ipm_enforce_gen.c
          # Fix result variable types
          sed -i 's/char \*err0 =/const char *err0 =/' ipm_enforce_gen.c
          sed -i 's/char \*err1 =/const char *err1 =/' ipm_enforce_gen.c
          sed -i 's/char \*err2 =/const char *err2 =/' ipm_enforce_gen.c
          sed -i 's/char \*err3 =/const char *err3 =/' ipm_enforce_gen.c
          sed -i '/const char \*err =/!s/char \*err =/const char *err =/' ipm_enforce_gen.c
          sed -i '/const char \*err =/!s/char \*err =/const char *err =/' ipm_enforce_gen.c
          sed -i 's/int errors = 0;/int errors = 0; (void)errors;/' ipm_enforce_gen.c

          
          $CC ${builtins.toString cflags} \
            -Isrc \
            -I${S} \
            -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -I${S}/enforce-structural-rules-for-code \
            -I${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns \
            src/support-code-for-compiled-output/file-string-and-json-parsing.c \
            src/support-code-for-compiled-output/hash-table-and-substitution-code.c \
            src/support-code-for-compiled-output/buffer-output-and-command-launch.c \
            src/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            src/support-code-for-compiled-output/structural-rule-checker-batch-two/structural-rule-checker-batch-two.c \
            src/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c \
            src/support-code-for-compiled-output/ipm-file-validator-ffi-batch/ipm-file-validator-ffi-batch.c \
            src/support-code-for-compiled-output/remaining-rules-ffi-batch-four/remaining-rules-ffi-batch-four.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/ffi-function-export-layer-here/enforce-ffi-function-export-layer.c \
            ipm_enforce_gen.c \
            -o ipm-enforce -lcjson
default = self.packages.${system}.spec2c;
    });
  };
}
# force rebuild
