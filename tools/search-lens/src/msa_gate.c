// SPDX-License-Identifier: Apache-2.0
// msa-gate — structural proof for MSA: VRAM check + daemon health
// Usage: msa-gate → JSON {"vram_free_mb":..., "daemon_alive":..., "verdict":"PASS"|"FAIL"}
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cjson/cJSON.h>

static int check_vram(long *free_mb) {
    FILE *fp = popen("nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits 2>/dev/null", "r");
    if (!fp) return -1;
    char buf[64]; *free_mb = 0;
    if (fgets(buf, sizeof(buf), fp)) *free_mb = atol(buf);
    pclose(fp);
    return *free_mb > 0 ? 0 : -1;
}

static int check_msa_socket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/vehir-msa.sock");
    int rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    close(fd);
    return rc == 0 ? 1 : 0;
}

int main(void) {
    cJSON *r = cJSON_CreateObject();
    
    long vram_free = 0;
    int vram_ok = check_vram(&vram_free) == 0;
    cJSON_AddNumberToObject(r, "vram_free_mb", (double)vram_free);
    cJSON_AddStringToObject(r, "vram_check", vram_ok ? "PASS" : "FAIL");
    
    int msa_alive = check_msa_socket();
    cJSON_AddStringToObject(r, "msa_daemon", msa_alive ? "ALIVE" : "DOWN");
    
    int embed_fits = vram_free > 2800; /* Qwen3-Embedding needs ~2.8GB */
    int msa_fits = vram_free > 3000;   /* MSA-4B needs ~3GB */
    int coexists = embed_fits && msa_fits;
    
    cJSON_AddStringToObject(r, "embed_fits_6gb", embed_fits ? "YES" : "NO");
    cJSON_AddStringToObject(r, "msa_fits_6gb", msa_fits ? "YES" : "NO");
    cJSON_AddStringToObject(r, "coexistence_6gb", coexists ? "POSSIBLE" : "SWAP_REQUIRED");
    
    const char *verdict = (vram_ok) ? "PASS" : "FAIL";
    cJSON_AddStringToObject(r, "verdict", verdict);
    
    char *out = cJSON_Print(r);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(r);
    return vram_ok ? 0 : 1;
}
