// SPDX-License-Identifier: Apache-2.0
#include "../../verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

extern char manifest_paths[128][256];
extern int manifest_paths_count;

void load_manifest_paths_for_exemption(const char *srcdir) {
    char *content = NULL;
    long content_len = 0;
    load_operator_integrity_manifest_file(srcdir, &content, &content_len);
    { cJSON *pre = cJSON_Parse(content);
      if (pre) {
        cJSON *e = cJSON_GetObjectItem(pre, "entries");
        if (e) { int sz = cJSON_GetArraySize(e);
          for (int i = 0; i < sz && manifest_paths_count < 128; i++) {
            cJSON *item = cJSON_GetArrayItem(e, i);
            cJSON *fn = cJSON_GetObjectItem(item, "file");
            if (fn && fn->valuestring)
              snprintf(manifest_paths[manifest_paths_count++], 256, "%s", fn->valuestring);
          }
        }
        cJSON_Delete(pre);
      }
    }
    free(content);
}
