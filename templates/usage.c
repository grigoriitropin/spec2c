_Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s{{config_opt}}\n"
        "\n"
        "{{description}}\n"
        "\n"
        "{{config_usage_block}}"
        "Output: JSON {\"ok\":true/false, ...}\n"
        "Exit:   0 = success, 1 = error, 2 = usage error\n",
        prog);
    exit(2);
}
