// SPDX-License-Identifier: Apache-2.0
int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
        usage(argv[0]);

{{config_init}}

{{db_init}}

    int rc = {{core_function}}(db, argc, argv{{core_params_call}});

{{config_cleanup}}
{{db_cleanup}}

    return rc;
}
