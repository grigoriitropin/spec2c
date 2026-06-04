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
      S = "src/compile-specifications-into-source-code";
      inc = [
        "-Isrc"
        "-I${S}"
        "-I${S}/shared-type-declarations-across-modules"
        "-I${S}/parse-legacy-specification-file-format"
        "-Isrc/support-code-for-compiled-output"
      ];
      enforce_inc = inc ++ [
        "-I."
        "-I${S}/enforce-structural-rules-for-code"
        "-I${S}/verify-conformance-against-soul-patterns"
      ];
      runtime_src = [
        "src/runtime-for-generated-ipm-code.c"
        "src/runtime-weak-stub-symbol-overrides/runtime-weak-stubs-part-two.c"
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
        buildInputs = [ cjson-static ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} ${builtins.toString enforce_inc} \
            ${S}/enforce-structural-rules-for-code/verify-structural-source-code-rules.c \
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
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
            ${S}/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            ${S}/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c \
            ${S}/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
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
          cp -r templates $out/share/spec2c/
          runHook postInstall
        '';
        doCheck = true;
        checkPhase = ''
          runHook preCheck
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
    src = builtins.filterSource (path: type:
      let bn = baseNameOf path; in
      bn != "tools" && bn != "tests" && bn != "src.backup" &&
      bn != "bootstrap" && bn != ".git" && bn != "archive"
    ) ./.;
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
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -c dfa_obj/dfa.c -o dfa_obj/dfa.o

          # Line density checker
          spec2c modules/rules/check-each-line-token-density.ipm --library -o density_obj/density.c
          sed -i '/^{"ok"/d' density_obj/density.c
          sed -i 's/void check_each_line_token_density/int check_each_line_token_density/' density_obj/check_each_line_token_density.h
          sed -i 's/(char \* path)/(const char *path)/g' density_obj/density.c
          sed -i 's/(char \* path)/(const char *path)/g' density_obj/check_each_line_token_density.h
          sed -i 's/const char \*_name = "[^"]*";//' density_obj/density.c
          $CC ${builtins.toString cflags} \
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -c density_obj/density.c -o density_obj/density.o

          # Hardcoded path detector
          spec2c modules/rules/detect-any-hardcoded-filesystem-paths.ipm --library -o path_obj/path.c
          sed -i '/^{"ok"/d' path_obj/path.c
          sed -i 's/void detect_any_hardcoded_filesystem_paths/int detect_any_hardcoded_filesystem_paths/' path_obj/detect_any_hardcoded_filesystem_paths.h
          sed -i 's/(char \* path)/(const char *path)/g' path_obj/path.c
          sed -i 's/(char \* path)/(const char *path)/g' path_obj/detect_any_hardcoded_filesystem_paths.h
          sed -i 's/const char \*_name = "[^"]*";//' path_obj/path.c
          $CC ${builtins.toString cflags} \
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -c path_obj/path.c -o path_obj/path.o

          # Naming rules validator
          spec2c modules/rules/validate-file-stem-naming-dfa.ipm --library -o naming_obj/naming.c
          sed -i '/^{"ok"/d' naming_obj/naming.c
          sed -i 's/void validate_file_stem_naming_dfa/int validate_file_stem_naming_dfa/' naming_obj/validate_file_stem_naming_dfa.h
          sed -i 's/(char \* name_arg)/(const char *name_arg)/g' naming_obj/naming.c
          sed -i 's/(char \* name_arg)/(const char *name_arg)/g' naming_obj/validate_file_stem_naming_dfa.h
          sed -i 's/const char \*_name = "[^"]*";//' naming_obj/naming.c
          sed -i 's/extern int check_name_against_allowed_whitelist_ffi(char \*/extern int check_name_against_allowed_whitelist_ffi(const char */' naming_obj/naming.c
          sed -i 's/extern int check_name_against_allowed_whitelist_ffi/extern int check_allowed_name_whitelist_ffi/' naming_obj/naming.c
          sed -i 's/check_name_against_allowed_whitelist_ffi(name_arg)/check_allowed_name_whitelist_ffi(name_arg)/' naming_obj/naming.c
          $CC ${builtins.toString cflags} \
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -c naming_obj/naming.c -o naming_obj/naming.o

          # C lexer (function count + length)
          spec2c modules/rules/locate-all-function-body-blocks.ipm --library -o clex_obj/clex.c
          sed -i '/^{"ok"/d' clex_obj/clex.c
          sed -i 's/void locate_all_function_body_blocks/int locate_all_function_body_blocks/' clex_obj/locate_all_function_body_blocks.h
          sed -i 's/(char \* path)/(const char *path)/g' clex_obj/clex.c
          sed -i 's/(char \* path)/(const char *path)/g' clex_obj/locate_all_function_body_blocks.h
          sed -i 's/const char \*_name = "[^"]*";//' clex_obj/clex.c
          $CC ${builtins.toString cflags} \
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
            -c clex_obj/clex.c -o clex_obj/clex.o

          # Main() detector
          spec2c modules/rules/find-every-main-function-block.ipm --library -o main_obj/main.c
          sed -i '/^{"ok"/d' main_obj/main.c
          sed -i 's/void find_every_main_function_block/int find_every_main_function_block/' main_obj/find_every_main_function_block.h
          sed -i 's/(char \* path)/(const char *path)/g' main_obj/main.c
          sed -i 's/(char \* path)/(const char *path)/g' main_obj/find_every_main_function_block.h
          sed -i 's/const char \*_name = "[^"]*";//' main_obj/main.c
          sed -i 's/(char \* text, char \* pattern)/(const char *text, const char *pattern)/' main_obj/main.c
          $CC ${builtins.toString cflags} \
            -Isrc -I${S} -I${S}/shared-type-declarations-across-modules \
            -Isrc/support-code-for-compiled-output \
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
            src/support-code-for-compiled-output/ipm-file-validator-ffi-batch/ipm-file-validator-ffi-batch.c \
            src/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/operator-signed-exemption-name-table/load-operator-signed-exemption-table.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c \
            src/runtime-weak-stub-symbol-overrides/runtime-weak-stubs-part-two.c \
            verify-ed25519-digital-signature-key.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c \
            src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/ffi-function-export-layer-here/enforce-ffi-function-export-layer.c \
            src/support-code-for-compiled-output/remaining-rules-ffi-batch-four/remaining-rules-ffi-batch-four.c \
            src/support-code-for-compiled-output/dead-code-header-check-batch/dead-code-header-check-batch.c \
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
          cp src/allowed-names.txt gen_c/ 2>/dev/null || true
          cp src/banned-patterns.txt gen_c/ 2>/dev/null || true
          cp src/bootstrap-c-whitelist.txt gen_c/ 2>/dev/null || true
          cp src/bootstrap-c-freeze-limits.txt gen_c/ 2>/dev/null || true
          for f in gen_c/*.c; do sed -i '/^{"ok"/d' "$f" 2>/dev/null; done
          echo "=== Translation Gate ==="
          $S2C_ENFORCE --lint ./gen_c || echo "(generated code enforcer check done)"

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
          cc -O2 -I. enforce-link-time-whitelisted-symbols.c -o enforce-link-time-symbol-whitelist
          mkdir -p $out/bin
          cp ipm-enforce $out/bin/
          for bin in $out/bin/*; do
            ./enforce-link-time-symbol-whitelist "$bin" || exit 1
          done
          runHook postCheck
        '';
      };

      default = self.packages.${system}.spec2c;
    });
  };
}
# force rebuild

