static int extract_signed_integrity_payload_bytes(const char *raw, long len, char *out, int outsz) {
    cJSON *root = cJSON_Parse(raw);
    if (!root) return 0;
    cJSON_DeleteItemFromObject(root, "signature_hex");
    char *payload = cJSON_PrintUnformatted(root);
    if (!payload) { cJSON_Delete(root); return 0; }
    int plen = (int)strlen(payload);
    if (plen < outsz) { memcpy(out, payload, plen); out[plen] = 0; }
    else plen = 0;
    free(payload);
    cJSON_Delete(root);
    return plen;
}