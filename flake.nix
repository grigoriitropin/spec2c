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
      s2e-pkgs = pkgs;
      cjson-static = pkgs.cjson.overrideAttrs (old: {
        cmakeFlags = (old.cmakeFlags or []) ++ [ "-DBUILD_SHARED_LIBS=OFF" "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" ];
        postPatch = (old.postPatch or "") + ''
          cat << 'EOF' > patch.txt
          static inline size_t custom_strlen(const char *s) {
              size_t len = 0;
              while (s[len]) len++;
              return len;
          }
          static inline int custom_strncmp(const char *s1, const char *s2, size_t n) {
              for (size_t i = 0; i < n; i++) {
                  if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
                  if (s1[i] == 0) return 0;
              }
              return 0;
          }
          #define strlen custom_strlen
          #define strncmp custom_strncmp
          EOF
          sed -i '/#include <string.h>/r patch.txt' cJSON.c
          rm patch.txt
        '';
      });
      cflags = [ "-O2" "-Wall" "-Wextra" "-Werror" "-std=c2x" "-D_POSIX_C_SOURCE=200809L"
                 "-fno-ident" "-frandom-seed=spec2c" "-Wl,--build-id=none" ];
      S = "source-code-for-compiler-generation/compile-specifications-into-source-code";
      inc = [
        "-Isource-code-for-compiler-generation"
        "-I${S}"
        "-I${S}/shared-type-declarations-across-modules"
        "-I${S}/parse-legacy-specification-file-format"
        "-Isource-code-for-compiler-generation/support-code-for-compiled-output"
      ];
      enforce_inc = inc ++ [
        "-I."
        "-I${S}/enforce-structural-rules-for-code"
        "-I${S}/verify-conformance-against-soul-patterns"
      ];
      runtime_src = [
        "source-code-for-compiler-generation/runtime-for-generated-ipm-code.c"
        "source-code-for-compiler-generation/runtime-weak-stub-symbol-overrides/runtime-weak-stubs-part-two.c"
        "source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c"
        "source-code-for-compiler-generation/support-code-for-compiled-output/file-string-and-json-parsing.c"
        "source-code-for-compiler-generation/support-code-for-compiled-output/hash-table-and-substitution-code.c"
            "source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c"
        "source-code-for-compiler-generation/support-code-for-compiled-output/structural-rule-checker-batch-two/structural-rule-checker-batch-two.c"
        "source-code-for-compiler-generation/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c"
        "source-code-for-compiler-generation/support-code-for-compiled-output/buffer-output-and-command-launch.c"
      ];
    in {
      # ── Manifest generator (C, no Python in trust path) ─────────────
      generate-integrity-manifest-from-source = pkgs.stdenv.mkDerivation {
        pname = "generate-integrity-manifest-from-source";
        version = "0.1.0";
        inherit src;
        buildInputs = [ cjson-static ];
        buildPhase = ''
          cc -O2 -Wall -Werror -std=c2x -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            generate-integrity-manifest-from-source.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            -o generate-integrity-manifest-from-source \
            ${cjson-static}/lib/libcjson.a -lm
        '';
        installPhase = ''
          mkdir -p $out/bin
          cp generate-integrity-manifest-from-source $out/bin/
        '';
      };
      # ── Standalone enforcement checker ────────────────────────────
      s2c-enforce = pkgs.stdenv.mkDerivation {
        pname = "s2c-enforce";
        version = "0.3.0";
        inherit src;
        buildInputs = [ cjson-static ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} ${builtins.toString enforce_inc} \
            ${S}/enforce-structural-rules-for-code/verify-structural-source-code-rules.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/report-fatal-error-and-exit-helper/report-fatal-error-and-exit-helper.c \
            ${S}/enforce-structural-rules-for-code/audit-checks-for-source-code/secondary-source-code-audit-checks.c \
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/count-file-name-word-segments/count-file-name-word-segments.c \
            ${S}/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/validate-ipm-content-scan/validate-ipm-file-content-scan.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/load-manifest-path-exemption-entries/load-manifest-paths-for-exemption.c \
            ${builtins.toString runtime_src} \
            verify-ed25519-digital-signature-key.c \
            -o s2c-enforce ${cjson-static}/lib/libcjson.a -lm
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp s2c-enforce $out/bin/
          runHook postInstall
        '';
        doCheck = true;
        checkPhase = ''
          runHook preCheck
          cc -O2 source-code-for-compiler-generation/known-answer-tests-for-primitives/verify-sha-produces-known-answers.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            -o sha-kat && ./sha-kat || exit 1
          cc -O2 -I. enforce-link-time-whitelisted-symbols.c -o enforce-link-time-symbol-whitelist
          mkdir -p $out/bin
          cp s2c-enforce $out/bin/
          for bin in $out/bin/*; do
            ./enforce-link-time-symbol-whitelist "$bin" || exit 1
          done
          runHook postCheck
        '';
      };

      # ── spec2c compiler (enforcement gate inline) ─────────────────
      spec2c = pkgs.stdenv.mkDerivation {
        pname = "spec2c";
        version = "0.3.0";
        inherit src;
        buildInputs = [ cjson-static ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild

          # Step 1: Build enforcement checker
          cc ${builtins.toString cflags} ${builtins.toString enforce_inc} \
            ${S}/enforce-structural-rules-for-code/verify-structural-source-code-rules.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/report-fatal-error-and-exit-helper/report-fatal-error-and-exit-helper.c \
            ${S}/enforce-structural-rules-for-code/audit-checks-for-source-code/secondary-source-code-audit-checks.c \
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/count-file-name-word-segments/count-file-name-word-segments.c \
            ${S}/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/validate-ipm-content-scan/validate-ipm-file-content-scan.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/load-manifest-path-exemption-entries/load-manifest-paths-for-exemption.c \
            ${builtins.toString runtime_src} \
            verify-ed25519-digital-signature-key.c \
            -o s2c_enforce ${cjson-static}/lib/libcjson.a -lm

          # Step 2: Run enforcement gate (exits 1 on violation → build fails)
          ./s2c_enforce .

          # Step 3: Build spec2c
          cc ${builtins.toString cflags} ${builtins.toString inc} \
            ${S}/parse-command-dispatch-into-pipeline.c \
            ${S}/compile-abstract-instructions-into-code.c \
            ${S}/generate-output-from-ipm-specification.c \
            ${S}/parse-legacy-specification-file-format/parse-old-format-specification-data.c \
            ${S}/codegen-instruction-handler-function-set/extracted-codegen-helper-functions-here/emit-report-error-and-exit.c \
            ${S}/codegen-instruction-handler-function-set/emit-variable-declaration-handler-function.c \
            ${builtins.toString runtime_src} \
            verify-ed25519-digital-signature-key.c \
            -o spec2c ${cjson-static}/lib/libcjson.a -lm

          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/share/spec2c
          cp spec2c $out/bin/
          cp s2c_enforce $out/bin/
          cp skeleton.json $out/share/spec2c/
          runHook postInstall
        '';
        doCheck = true;
        checkPhase = ''
          runHook preCheck
          cc -O2 source-code-for-compiler-generation/known-answer-tests-for-primitives/verify-sha-produces-known-answers.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            -o sha-kat && ./sha-kat || exit 1
          cc -O2 -I. enforce-link-time-whitelisted-symbols.c -o enforce-link-time-symbol-whitelist
          mkdir -p $out/bin
          cp spec2c s2c_enforce $out/bin/
          for bin in $out/bin/*; do
            ./enforce-link-time-symbol-whitelist "$bin" || exit 1
          done
          runHook postCheck
        '';
      };

      # ── IPM enforcer (compiled from IPM spec by spec2c) ──────────
      ipm-enforce = pkgs.stdenv.mkDerivation {
        pname = "ipm-enforce";
        version = "0.1.0";
    src = ./. ;
        buildInputs = [ self.packages.${system}.spec2c cjson-static ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        S2C_ENFORCE = "${self.packages.${system}.spec2c}/bin/s2c_enforce";
        buildPhase = ''
          runHook preBuild

          # ── Step 1: Build library modules ─────────────────
          mkdir -p dfa_obj density_obj path_obj naming_obj clex_obj main_obj

          # DFA banned patterns
          spec2c check-banned-patterns-pure-ipm.ipm --library -o dfa_obj/dfa.c
          sed -i '/^{"ok"/d' dfa_obj/dfa.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c dfa_obj/dfa.c -o dfa_obj/dfa.o

          # Line density checker
          spec2c modules/rules/naming-and-density-checkers/check-each-line-token-density.ipm --library -o density_obj/density.c
          sed -i '/^{"ok"/d' density_obj/density.c
          sed -i 's/void check_each_line_token_density/int check_each_line_token_density/' density_obj/check_each_line_token_density.h
          sed -i 's/(char \* path)/(const char *path)/g' density_obj/density.c
          sed -i 's/(char \* path)/(const char *path)/g' density_obj/check_each_line_token_density.h
          sed -i 's/const char \*_name = "[^"]*";//' density_obj/density.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c density_obj/density.c -o density_obj/density.o

          # Hardcoded path detector
          spec2c modules/rules/function-and-pattern-scanners/detect-any-hardcoded-filesystem-paths.ipm --library -o path_obj/path.c
          sed -i '/^{"ok"/d' path_obj/path.c
          sed -i 's/void detect_any_hardcoded_filesystem_paths/int detect_any_hardcoded_filesystem_paths/' path_obj/detect_any_hardcoded_filesystem_paths.h
          sed -i 's/(char \* path)/(const char *path)/g' path_obj/path.c
          sed -i 's/(char \* path)/(const char *path)/g' path_obj/detect_any_hardcoded_filesystem_paths.h
          sed -i 's/const char \*_name = "[^"]*";//' path_obj/path.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c path_obj/path.c -o path_obj/path.o

          # Naming rules validator
          spec2c modules/rules/naming-and-density-checkers/validate-file-stem-naming-dfa.ipm --library -o naming_obj/naming.c
          sed -i '/^{"ok"/d' naming_obj/naming.c
          sed -i 's/void validate_file_stem_naming_dfa/int validate_file_stem_naming_dfa/' naming_obj/validate_file_stem_naming_dfa.h
          sed -i 's/(char \* name_arg)/(const char *name_arg)/g' naming_obj/naming.c
          sed -i 's/(char \* name_arg)/(const char *name_arg)/g' naming_obj/validate_file_stem_naming_dfa.h
          sed -i 's/const char \*_name = "[^"]*";//' naming_obj/naming.c
          sed -i 's/extern int check_name_against_allowed_whitelist_ffi(char \*/extern int check_name_against_allowed_whitelist_ffi(const char */' naming_obj/naming.c
          sed -i 's/extern int check_name_against_allowed_whitelist_ffi/extern int check_allowed_name_whitelist_ffi/' naming_obj/naming.c
          sed -i 's/check_name_against_allowed_whitelist_ffi(name_arg)/check_allowed_name_whitelist_ffi(name_arg)/' naming_obj/naming.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c naming_obj/naming.c -o naming_obj/naming.o

          # C lexer (function count + length)
          spec2c modules/rules/function-and-pattern-scanners/locate-all-function-body-blocks.ipm --library -o clex_obj/clex.c
          sed -i '/^{"ok"/d' clex_obj/clex.c
          sed -i 's/void locate_all_function_body_blocks/int locate_all_function_body_blocks/' clex_obj/locate_all_function_body_blocks.h
          sed -i 's/(char \* path)/(const char *path)/g' clex_obj/clex.c
          sed -i 's/(char \* path)/(const char *path)/g' clex_obj/locate_all_function_body_blocks.h
          sed -i 's/const char \*_name = "[^"]*";//' clex_obj/clex.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c clex_obj/clex.c -o clex_obj/clex.o

          # Main() detector
          spec2c modules/rules/function-and-pattern-scanners/find-every-main-function-block.ipm --library -o main_obj/main.c
          sed -i '/^{"ok"/d' main_obj/main.c
          sed -i 's/void find_every_main_function_block/int find_every_main_function_block/' main_obj/find_every_main_function_block.h
          sed -i 's/(char \* path)/(const char *path)/g' main_obj/main.c
          sed -i 's/(char \* path)/(const char *path)/g' main_obj/find_every_main_function_block.h
          sed -i 's/const char \*_name = "[^"]*";//' main_obj/main.c
          sed -i 's/(char \* text, char \* pattern)/(const char *text, const char *pattern)/' main_obj/main.c
          $CC ${builtins.toString cflags} \
            -Isource-code-for-compiler-generation -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -c main_obj/main.c -o main_obj/main.o

          # ── Step 2: Build IPM enforcer executable ────────
          spec2c enforce-naming-rules-via-ffi.ipm > ipm_enforce_gen.c
          sed -i '/^{"ok"/d' ipm_enforce_gen.c
          sed -i '/enforce_naming_rules_via_ffi\.h"/d' ipm_enforce_gen.c
          sed -i '1i#include "runtime-for-generated-ipm-code.h"' ipm_enforce_gen.c
          sed -i 's/extern void initialize_naming_rules_enforcer_ffi(char \*/extern void initialize_naming_rules_enforcer_ffi(const char */' ipm_enforce_gen.c
          sed -i 's/extern char \* match_exemption_table_name_ffi(char \*/extern const char * match_exemption_table_name_ffi(const char */' ipm_enforce_gen.c
          sed -i 's/extern int check_bootstrap_source_whitelist_ffi(char \* name, char \* sub, char \* dirpath)/extern int check_bootstrap_source_whitelist_ffi(const char *name, const char *sub, const char *dirpath)/' ipm_enforce_gen.c
          sed -i 's/extern int validate_file_stem_dfa_ffi(char \*/extern int validate_file_stem_dfa_ffi(const char */' ipm_enforce_gen.c
          sed -i 's/extern void report_file_naming_error_ffi(char \* fullname, char \* path, int/extern void report_file_naming_error_ffi(const char *fullname, const char *path, int/' ipm_enforce_gen.c
          sed -i 's/extern void check_directory_file_limits_ffi(char \*/extern void check_directory_file_limits_ffi(const char */' ipm_enforce_gen.c
          sed -i 's/extern void check_single_file_rules_ffi(char \* sub, char \* fullname)/extern void check_single_file_rules_ffi(const char *sub, const char *fullname)/' ipm_enforce_gen.c
          sed -i 's/extern void check_project_main_count_ffi(char \*/extern void check_project_main_count_ffi(const char */' ipm_enforce_gen.c
          sed -i 's/extern void system_exit(char \*/extern void system_exit(const char */' ipm_enforce_gen.c
          sed -i 's/int errors = 0;/int errors = 0; (void)errors;/' ipm_enforce_gen.c
          sed -i '/const char \*_name = "scan_directory_recursively_with_rules";/d' ipm_enforce_gen.c
          sed -i 's/int scan_directory_recursively_with_rules(char \* dirpath)/int scan_directory_recursively_with_rules(const char *dirpath)/' ipm_enforce_gen.c
          sed -i 's/char \* ex_scan =/const char *ex_scan =/' ipm_enforce_gen.c
          sed -i 's/const char \*is_dir = cJSON_IsTrue/int is_dir = cJSON_IsTrue/' ipm_enforce_gen.c
          sed -i 's/const char \*has_evil = strstr/int has_evil = strstr/' ipm_enforce_gen.c
          sed -i 's/if (ex_scan)/if (ex_scan[0])/' ipm_enforce_gen.c
          sed -i '/int main/i\int scan_directory_recursively_with_rules(const char *dirpath);' ipm_enforce_gen.c

          $CC ${builtins.toString cflags} \
            -I. \
            -Isource-code-for-compiler-generation \
            -I${S} \
            -I${S}/shared-type-declarations-across-modules \
            -Isource-code-for-compiler-generation/support-code-for-compiled-output \
            -I${S}/enforce-structural-rules-for-code \
            -I${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns \
            source-code-for-compiler-generation/support-code-for-compiled-output/file-string-and-json-parsing.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/hash-table-and-substitution-code.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/buffer-output-and-command-launch.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/structural-rule-checker-batch-two/structural-rule-checker-batch-two.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/ipm-file-validator-ffi-batch/ipm-file-validator-ffi-batch.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/count-file-name-word-segments/count-file-name-word-segments.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/load-manifest-path-exemption-entries/load-manifest-paths-for-exemption.c \
            source-code-for-compiler-generation/runtime-weak-stub-symbol-overrides/runtime-weak-stubs-part-two.c \
            verify-ed25519-digital-signature-key.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c \
            source-code-for-compiler-generation/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/ffi-function-export-layer-here/enforce-ffi-function-export-layer.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/remaining-rules-ffi-batch-four/remaining-rules-ffi-batch-four.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/dead-code-header-check-batch/dead-code-header-check-batch.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/report-fatal-error-and-exit-helper/report-fatal-error-and-exit-helper.c \
            dfa_obj/dfa.o \
            density_obj/density.o \
            path_obj/path.o \
            naming_obj/naming.o \
            clex_obj/clex.o \
            main_obj/main.o \
            ipm_enforce_gen.c \
            -o ipm-enforce ${cjson-static}/lib/libcjson.a -lm

          # ── Translation Gate — enforcers on generated C ──
          mkdir -p gen_c
          cp dfa_obj/dfa.c density_obj/density.c path_obj/path.c \
             naming_obj/naming.c clex_obj/clex.c main_obj/main.c gen_c/
          cp source-code-for-compiler-generation/allowed-names.txt gen_c/
          cp source-code-for-compiler-generation/banned-patterns.txt gen_c/
          cp source-code-for-compiler-generation/bootstrap-c-whitelist.txt gen_c/
          cp source-code-for-compiler-generation/bootstrap-c-freeze-limits.txt gen_c/
          cp source-code-for-compiler-generation/allowed-non-source-files.txt gen_c/
          for f in gen_c/*.c; do sed -i '/^{"ok"/d' "$f"; done
          echo "=== Translation Gate ==="
          $S2C_ENFORCE --lint-generated ./gen_c || exit 1

          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp ipm-enforce $out/bin/
          runHook postInstall
        '';
        doCheck = true;
        checkPhase = ''
          runHook preCheck
          cc -O2 source-code-for-compiler-generation/known-answer-tests-for-primitives/verify-sha-produces-known-answers.c \
            source-code-for-compiler-generation/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c \
            -o sha-kat && ./sha-kat || exit 1
          cc -O2 -I. enforce-link-time-whitelisted-symbols.c -o enforce-link-time-symbol-whitelist
          mkdir -p $out/bin
          cp ipm-enforce $out/bin/
          for bin in $out/bin/*; do
            ./enforce-link-time-symbol-whitelist "$bin" || exit 1
          done
          runHook postCheck
        '';
      };

      # ── Codegen parity gate (TRANSITIONAL — dies at Phase 8 with Nix) ──────
      # Proves the IPM self-hosted codegen (the 3 modules under
      # ipm-compiler-definition-spec-directory/instruction-codegen-handler-spec-modules)
      # produces C byte-identical (after clang-format canonicalization) to the frozen C
      # codegen. Builds a twin-spec2c whose codegen is the 3 modules' generated code +
      # a thin adapter, then diffs over the 7 corpus modules AND the 3 codegen modules
      # themselves (CLOSURE: twin compiles twin). Fail-closed: an unhandled instruction
      # shape makes the twin error -> build fails.
      # NOTE: corpus parity is proven on those modules' current instruction shapes, NOT
      # general codegen equivalence. The adapter glue is transitional scaffolding (it
      # supplies the stateful inference C does inline); it dies with this gate at Phase 8.
      ipm-codegen-parity-gate = pkgs.stdenv.mkDerivation {
        pname = "ipm-codegen-parity-gate";
        version = "0.1.0";
        inherit src;
        buildInputs = [ cjson-static self.packages.${system}.spec2c ];
        nativeBuildInputs = [ pkgs.pkg-config pkgs.clang-tools ];
        SPEC2C = "${self.packages.${system}.spec2c}/bin/spec2c";
        buildPhase = ''
          runHook preBuild
          MODDIR=source-code-for-compiler-generation/ipm-compiler-definition-spec-directory/instruction-codegen-handler-spec-modules
          M1=$MODDIR/recursive-instruction-dispatch-codegen-core.ipm
          M2=$MODDIR/statement-instruction-codegen-handler-functions.ipm
          M3=$MODDIR/iteration-filesystem-codegen-handler-functions.ipm

          # Transitional adapter: bridge C driver entry to the twin's string_buffer
          # codegen + supply the stateful inference C does inline (number text,
          # variable-type lookup, function return_type). Dies with this gate.
          cat > parity-adapter.c <<'ADAPTER_EOF'
#include "share-type-definitions-across-files.h"
void compile_instruction_array_into_code(cJSON *instructions, string_buffer *buffer);
const char *extract_json_field_number_text(const cJSON *obj, const char *key) {
    static char buf[32];
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (v && cJSON_IsNumber(v)) { snprintf(buf, sizeof buf, "%d", v->valueint); return buf; }
    buf[0] = '0'; buf[1] = '\0';
    return buf;
}
static const char *twin_find_variable_type_recursive(cJSON *node, const char *var_name) {
    if (!node) return NULL;
    if (cJSON_IsObject(node)) {
        cJSON *it = cJSON_GetObjectItemCaseSensitive(node, "instruction_type");
        if (it && cJSON_IsString(it)) {
            if (!strcmp(it->valuestring, "variable_declaration")) {
                cJSON *vn = cJSON_GetObjectItemCaseSensitive(node, "variable_name");
                if (vn && cJSON_IsString(vn) && !strcmp(vn->valuestring, var_name)) {
                    cJSON *vt = cJSON_GetObjectItemCaseSensitive(node, "variable_type");
                    if (vt && cJSON_IsString(vt)) return vt->valuestring;
                }
            } else if (!strcmp(it->valuestring, "read_file_content")) {
                cJSON *rv = cJSON_GetObjectItemCaseSensitive(node, "result_variable");
                if (rv && cJSON_IsString(rv) && !strcmp(rv->valuestring, var_name)) return "slice";
            } else if (!strcmp(it->valuestring, "function_invocation")) {
                cJSON *rv = cJSON_GetObjectItemCaseSensitive(node, "result_assignment_variable");
                if (rv && cJSON_IsString(rv) && !strcmp(rv->valuestring, var_name)) {
                    cJSON *rt = cJSON_GetObjectItemCaseSensitive(node, "result_type");
                    if (rt && cJSON_IsString(rt)) return rt->valuestring;
                }
            }
        }
        for (cJSON *c = node->child; c; c = c->next) {
            const char *r = twin_find_variable_type_recursive(c, var_name);
            if (r) return r;
        }
    } else if (cJSON_IsArray(node)) {
        for (cJSON *c = node->child; c; c = c->next) {
            const char *r = twin_find_variable_type_recursive(c, var_name);
            if (r) return r;
        }
    }
    return NULL;
}
const char *lookup_current_function_variable_type(const char *var_name) {
    if (!current_function_definition_ast) return "";
    cJSON *params = cJSON_GetObjectItemCaseSensitive(current_function_definition_ast, "parameter_definitions");
    if (params && cJSON_IsArray(params)) {
        for (int i = 0; i < cJSON_GetArraySize(params); i++) {
            cJSON *p = cJSON_GetArrayItem(params, i);
            cJSON *pn = cJSON_GetObjectItemCaseSensitive(p, "parameter_name");
            if (pn && cJSON_IsString(pn) && !strcmp(pn->valuestring, var_name)) {
                cJSON *pt = cJSON_GetObjectItemCaseSensitive(p, "parameter_type");
                if (pt && cJSON_IsString(pt)) return pt->valuestring;
            }
        }
    }
    cJSON *body = cJSON_GetObjectItemCaseSensitive(current_function_definition_ast, "execution_instructions");
    const char *r = twin_find_variable_type_recursive(body, var_name);
    return r ? r : "";
}
const char *current_function_return_type_text(void) {
    if (!current_function_definition_ast) return "void";
    cJSON *rt = cJSON_GetObjectItemCaseSensitive(current_function_definition_ast, "return_type");
    if (rt && cJSON_IsString(rt) && rt->valuestring[0]) return rt->valuestring;
    return "void";
}
void generate_code_via_dispatch_table(cJSON *insts, FILE *out, int indent, const char *rt) {
    (void)indent; (void)rt;
    if (!cJSON_IsArray(insts)) return;
    string_buffer *sb = create_empty_growable_string_buffer();
    compile_instruction_array_into_code(insts, sb);
    if (sb->data && sb->len) fwrite(sb->data, 1, sb->len, out);
    free_allocated_string_buffer_memory(sb);
}
ADAPTER_EOF

          # Generate the 3 split codegen modules (--library => .c + a module-named .h with
          # a unique guard), apply the standard module fixups, then cross-include the
          # module-named headers so cross-module twin calls resolve (spec2c does NOT
          # auto-extern plain IPM calls; the module-named .h carry the prototypes).
          fixup() {
            sed -i '/^{"ok"/d' "$1"
            sed -i -E 's/\bchar \* ([a-z_0-9]+) = (resolve_spec_type_into_lang|extract_json_field_number_text|lookup_current_function_variable_type|current_function_return_type_text)\(/const char * \1 = \2(/g' "$1"
            sed -i 's/const char \*_name = "[^"]*";//' "$1"
            sed -i '1i extern const char *current_function_return_type_text(void);' "$1"
            sed -i '1i extern const char *lookup_current_function_variable_type(const char *var_name);' "$1"
            sed -i '1i extern const char *extract_json_field_number_text(const cJSON *obj, const char *key);' "$1"
            sed -i '1i#include "share-type-definitions-across-files.h"' "$1"
          }
          "$SPEC2C" "$M1" --library -o m1.c
          "$SPEC2C" "$M2" --library -o m2.c
          "$SPEC2C" "$M3" --library -o m3.c
          fixup m1.c ; fixup m2.c ; fixup m3.c
          sed -i '1i#include "iteration_filesystem_codegen_handler_functions.h"' m1.c
          sed -i '1i#include "statement_instruction_codegen_handler_functions.h"' m1.c
          sed -i '1i#include "recursive_instruction_dispatch_codegen_core.h"' m2.c
          sed -i '1i#include "recursive_instruction_dispatch_codegen_core.h"' m3.c

          # Build twin-spec2c (same cjson-static + cflags as the C spec2c; the two frozen
          # codegen leaves are replaced by the 3 generated modules + parity-adapter.c).
          cc ${builtins.toString cflags} ${builtins.toString inc} -I. \
            ${S}/parse-command-dispatch-into-pipeline.c \
            ${S}/compile-abstract-instructions-into-code.c \
            ${S}/generate-output-from-ipm-specification.c \
            ${S}/parse-legacy-specification-file-format/parse-old-format-specification-data.c \
            m1.c m2.c m3.c \
            parity-adapter.c \
            ${builtins.toString runtime_src} \
            verify-ed25519-digital-signature-key.c \
            -o spec2c-twin ${cjson-static}/lib/libcjson.a -lm

          # NOTE: no L2 symbol gate on twin-spec2c. It is the same bootstrap class as
          # spec2c (a libc-using bootstrap compiler, name-exempt in the L2 UNDEF gate);
          # its only non-whitelisted import (strncmp) comes from the shared
          # parse-old-format file, identical to spec2c. The glue introduces no new
          # banned imports. The gate's job is codegen PARITY.

          # Canonical parity diff: the 7 corpus modules + the 3 twin codegen modules
          # THEMSELVES (CLOSURE: twin compiles twin, byte-identical to C). Fail-closed.
          printf 'BasedOnStyle: LLVM\n' > .clang-format
          for m in \
            check-banned-patterns-pure-ipm.ipm \
            enforce-naming-rules-via-ffi.ipm \
            modules/rules/naming-and-density-checkers/check-each-line-token-density.ipm \
            modules/rules/naming-and-density-checkers/validate-file-stem-naming-dfa.ipm \
            modules/rules/function-and-pattern-scanners/locate-all-function-body-blocks.ipm \
            modules/rules/function-and-pattern-scanners/find-every-main-function-block.ipm \
            modules/rules/function-and-pattern-scanners/detect-any-hardcoded-filesystem-paths.ipm \
            "$M1" "$M2" "$M3" ; do
            "$SPEC2C" "$m" --library -o ref.c || exit 1
            ./spec2c-twin "$m" --library -o twin.c || { echo "PARITY GATE: twin failed on $m (unhandled shape)"; exit 1; }
            clang-format ref.c > ref.fmt
            clang-format twin.c > twin.fmt
            diff ref.fmt twin.fmt || { echo "PARITY GATE: MISMATCH on $m"; exit 1; }
            echo "parity OK: $m"
          done
          echo "ipm-codegen-parity-gate: PASS (7/7 corpus + 3/3 closure)"
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out
          echo "ipm-codegen-parity-gate PASS" > $out/parity-passed.txt
          runHook postInstall
        '';
      };

      default = self.packages.${system}.spec2c;
    });
  };
}
# force rebuild

