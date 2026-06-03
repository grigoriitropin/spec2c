#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
static void scan_scope(const char *dpath, char **man, int mc) {
    DIR *d = opendir(dpath); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dpath, de->d_name);
        struct stat st; if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { scan_scope(sub, man, mc); continue; }
        size_t nl = strlen(de->d_name);
        int is_src = (nl > 2 && !strcmp(de->d_name + nl - 2, ".c"))
                  || (nl > 4 && !strcmp(de->d_name + nl - 4, ".ipm"));
        if (!is_src) continue;
        const char *p = sub; while (*p == '.' || *p == '/') p++;
        int found = 0;
        for (int i = 0; i < mc; i++) if (man[i] && !strcmp(p, man[i])) { found = 1; break; }
        if (!found) { fprintf(stderr, "spec2c: source outside sanctioned tree: %s\n", sub); exit(1); }
    }
    closedir(d);
}
int main(void) {
    char *man[256] = {0}; int mc = 0;
    FILE *mf = fopen("source-manifest.json", "r");
    if (!mf) return 0;
    fseek(mf, 0, SEEK_END); long ms = ftell(mf); fseek(mf, 0, SEEK_SET);
    char *mb = malloc(ms + 1);
    if (!mb) { fclose(mf); return 0; }
    size_t nr = fread(mb, 1, ms, mf); fclose(mf); mb[nr] = 0;
    cJSON *arr = cJSON_Parse(mb); free(mb);
    if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return 0; }
    int ac = cJSON_GetArraySize(arr);
    for (int i = 0; i < ac && mc < 256; i++) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsString(it)) man[mc++] = strdup(it->valuestring);
    }
    cJSON_Delete(arr);
    if (!mc) return 0;
    scan_scope(".", man, mc);
    return 0;
}
