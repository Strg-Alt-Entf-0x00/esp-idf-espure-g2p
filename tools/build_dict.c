/**
 * @file build_dict.c
 * @brief Standalone, zero-dependency C host tool to compile G2P dictionaries.
 *
 * Compiles raw JSON exception lexicons into either:
 *   1. Plain binary-search sorted C arrays (de_DE_dict.c)
 *   2. Front-coded Huffman-compressed binary ROM blobs (de_DE_dict_compressed.c)
 *
 * Designed to compile in < 0.1s on any C99 compiler (GCC, Clang, MSVC)
 * with zero external dependencies (no Python, no npm, no external libs).
 *
 * Usage:
 *   build_dict --locale de-DE --format plain
 *   build_dict --locale de-DE --format huffman
 *   build_dict --locale en-GB --format plain
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_ENTRIES 16384
#define MAX_WORD_LEN 128
#define MAX_PHONEME_LEN 256
#define BLOCK_SIZE 128

typedef struct {
    char word[MAX_WORD_LEN];
    char phonemes[MAX_PHONEME_LEN];
    char norm_word[MAX_WORD_LEN];
} dict_entry_raw_t;

typedef struct {
    char locale[16];
    char data_key[16];
    char input_file[64];
    char output_plain[128];
    char output_huffman[128];
    char symbol_plain[32];
    char description[64];
} locale_config_t;

static const locale_config_t CONFIGS[] = {
    {
        "de-DE", "de_DE", "de_DE_espeak_exceptions.json",
        "lang/de_DE/de_DE_dict.c", "lang/de_DE/de_DE_dict_compressed.c",
        "DICT_DE_DE", "German exception"
    },
    {
        "en-GB", "en_GB", "en_GB_pilot_exceptions.json",
        "lang/en_GB/en_GB_dict.c", "lang/en_GB/en_GB_dict_compressed.c",
        "DICT_EN_GB", "en-GB architecture-pilot"
    }
};

/* -------------------------------------------------------------------------
 * Normalization
 * ------------------------------------------------------------------------- */
static void normalize_german_word(const char* in, char* out, size_t out_size) {
    size_t i = 0, j = 0;
    size_t in_len = strlen(in);
    while (i < in_len && j + 4 < out_size) {
        unsigned char c = (unsigned char)in[i];
        if (c >= 'A' && c <= 'Z') {
            out[j++] = (char)(c + 32);
            i++;
        } else if (c == 0xC3 && i + 1 < in_len) {
            unsigned char c2 = (unsigned char)in[i + 1];
            if (c2 == 0x84) { out[j++] = (char)0xC3; out[j++] = (char)0xA4; i += 2; } // Ä -> ä
            else if (c2 == 0x96) { out[j++] = (char)0xC3; out[j++] = (char)0xB6; i += 2; } // Ö -> ö
            else if (c2 == 0x9C) { out[j++] = (char)0xC3; out[j++] = (char)0xBC; i += 2; } // Ü -> ü
            else if (c2 >= 0x80 && c2 <= 0x96) { out[j++] = (char)0xC3; out[j++] = (char)(c2 + 0x20); i += 2; }
            else if (c2 >= 0x98 && c2 <= 0x9E) { out[j++] = (char)0xC3; out[j++] = (char)(c2 + 0x20); i += 2; }
            else { out[j++] = (char)c; out[j++] = (char)c2; i += 2; }
        } else if (c == 0xE1 && i + 2 < in_len && (unsigned char)in[i+1] == 0xBA && (unsigned char)in[i+2] == 0x9E) {
            // Capital ẞ (U+1E9E) -> ß (0xC3 0x9F)
            out[j++] = (char)0xC3;
            out[j++] = (char)0x9F;
            i += 3;
        } else {
            out[j++] = (char)c;
            i++;
        }
    }
    out[j] = '\0';
}

static void normalize_english_word(const char* in, char* out, size_t out_size) {
    size_t i = 0;
    while (in[i] && i + 1 < out_size) {
        out[i] = (char)tolower((unsigned char)in[i]);
        i++;
    }
    out[i] = '\0';
}

static void normalize_word(const char* locale, const char* in, char* out, size_t out_size) {
    if (strcmp(locale, "de-DE") == 0) {
        normalize_german_word(in, out, out_size);
    } else {
        normalize_english_word(in, out, out_size);
    }
}

/* -------------------------------------------------------------------------
 * Minimal JSON Parser for {"word": "phonemes", ...}
 * ------------------------------------------------------------------------- */
static const char* skip_ws(const char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

static const char* parse_json_string(const char* p, char* out, size_t max_out) {
    if (*p != '"') return NULL;
    p++;
    size_t idx = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
            if (*p == '"') { if (idx + 1 < max_out) out[idx++] = '"'; p++; }
            else if (*p == '\\') { if (idx + 1 < max_out) out[idx++] = '\\'; p++; }
            else if (*p == '/') { if (idx + 1 < max_out) out[idx++] = '/'; p++; }
            else if (*p == 'n') { if (idx + 1 < max_out) out[idx++] = '\n'; p++; }
            else if (*p == 'r') { if (idx + 1 < max_out) out[idx++] = '\r'; p++; }
            else if (*p == 't') { if (idx + 1 < max_out) out[idx++] = '\t'; p++; }
            else if (*p == 'u') {
                p++;
                char hex[5] = {0};
                for (int h = 0; h < 4 && isxdigit((unsigned char)*p); h++, p++) hex[h] = *p;
                long codepoint = strtol(hex, NULL, 16);
                if (codepoint <= 0x7F) {
                    if (idx + 1 < max_out) out[idx++] = (char)codepoint;
                } else if (codepoint <= 0x7FF) {
                    if (idx + 2 < max_out) {
                        out[idx++] = (char)(0xC0 | ((codepoint >> 6) & 0x1F));
                        out[idx++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                } else {
                    if (idx + 3 < max_out) {
                        out[idx++] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
                        out[idx++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        out[idx++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                }
            } else {
                if (idx + 1 < max_out) out[idx++] = *p;
                p++;
            }
        } else {
            if (idx + 1 < max_out) out[idx++] = *p;
            p++;
        }
    }
    if (*p != '"') return NULL;
    p++;
    out[idx] = '\0';
    return p;
}

static int parse_json_dict(const char* json, dict_entry_raw_t* entries, int max_entries) {
    const char* p = skip_ws(json);
    if (*p != '{') return -1;
    p++;
    int count = 0;

    while (*p) {
        p = skip_ws(p);
        if (*p == '}') { p++; break; }
        if (count >= max_entries) break;

        char key[MAX_WORD_LEN];
        p = parse_json_string(p, key, sizeof(key));
        if (!p) return -1;

        p = skip_ws(p);
        if (*p != ':') return -1;
        p++;

        p = skip_ws(p);
        char val[MAX_PHONEME_LEN];
        p = parse_json_string(p, val, sizeof(val));
        if (!p) return -1;

        strncpy(entries[count].word, key, sizeof(entries[count].word) - 1);
        entries[count].word[sizeof(entries[count].word) - 1] = '\0';
        strncpy(entries[count].phonemes, val, sizeof(entries[count].phonemes) - 1);
        entries[count].phonemes[sizeof(entries[count].phonemes) - 1] = '\0';
        count++;

        p = skip_ws(p);
        if (*p == ',') {
            p++;
        } else if (*p == '}') {
            p++;
            break;
        } else {
            return -1;
        }
    }
    return count;
}

/* -------------------------------------------------------------------------
 * C String Escaping
 * ------------------------------------------------------------------------- */
static void write_c_string(FILE* f, const char* str) {
    fputc('"', f);
    while (*str) {
        unsigned char c = (unsigned char)*str++;
        if (c == '\\') fputs("\\\\", f);
        else if (c == '"') fputs("\\\"", f);
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c < 0x20 || c == 0x7F) fprintf(f, "\\%03o", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

/* -------------------------------------------------------------------------
 * Sorting
 * ------------------------------------------------------------------------- */
static int compare_entries(const void* a, const void* b) {
    const dict_entry_raw_t* ea = (const dict_entry_raw_t*)a;
    const dict_entry_raw_t* eb = (const dict_entry_raw_t*)b;
    int cmp = strcmp(ea->norm_word, eb->norm_word);
    if (cmp != 0) return cmp;
    return strcmp(ea->word, eb->word);
}

static int compare_words_only(const void* a, const void* b) {
    const dict_entry_raw_t* ea = (const dict_entry_raw_t*)a;
    const dict_entry_raw_t* eb = (const dict_entry_raw_t*)b;
    return strcmp(ea->word, eb->word);
}

/* -------------------------------------------------------------------------
 * Plain Dictionary Generator
 * ------------------------------------------------------------------------- */
static int generate_plain(const locale_config_t* cfg, dict_entry_raw_t* raw, int raw_count, const char* out_path) {
    for (int i = 0; i < raw_count; i++) {
        normalize_word(cfg->locale, raw[i].word, raw[i].norm_word, sizeof(raw[i].norm_word));
    }

    /* Case grouping: if all variants of normalized word have identical pronunciation, collapse to normalized */
    qsort(raw, raw_count, sizeof(dict_entry_raw_t), compare_entries);

    dict_entry_raw_t* final_entries = (dict_entry_raw_t*)malloc(sizeof(dict_entry_raw_t) * raw_count);
    if (!final_entries) return -1;
    int final_count = 0;

    int i = 0;
    while (i < raw_count) {
        int j = i + 1;
        bool all_same_ph = true;
        while (j < raw_count && strcmp(raw[i].norm_word, raw[j].norm_word) == 0) {
            if (strcmp(raw[i].phonemes, raw[j].phonemes) != 0) {
                all_same_ph = false;
            }
            j++;
        }
        if (all_same_ph) {
            strcpy(final_entries[final_count].word, raw[i].norm_word);
            strcpy(final_entries[final_count].norm_word, raw[i].norm_word);
            strcpy(final_entries[final_count].phonemes, raw[i].phonemes);
            final_count++;
        } else {
            for (int k = i; k < j; k++) {
                strcpy(final_entries[final_count].word, raw[k].word);
                strcpy(final_entries[final_count].norm_word, raw[k].norm_word);
                strcpy(final_entries[final_count].phonemes, raw[k].phonemes);
                final_count++;
            }
        }
        i = j;
    }

    qsort(final_entries, final_count, sizeof(dict_entry_raw_t), compare_entries);

    FILE* out = fopen(out_path, "w");
    if (!out) {
        free(final_entries);
        return -1;
    }

    fprintf(out, "#include \"espure_internal.h\"\n\n");
    fprintf(out, "/**\n * @brief Generated %s dictionary.\n */\n", cfg->description);
    fprintf(out, "extern const espure_dict_entry_t %s[];\n", cfg->symbol_plain);
    fprintf(out, "extern const size_t %s_SIZE;\n\n", cfg->symbol_plain);
    fprintf(out, "const espure_dict_entry_t %s[] = {\n", cfg->symbol_plain);

    for (int k = 0; k < final_count; k++) {
        fprintf(out, "    {");
        write_c_string(out, final_entries[k].word);
        fprintf(out, ", ");
        write_c_string(out, final_entries[k].phonemes);
        fprintf(out, ", 0}%s\n", (k + 1 < final_count) ? "," : "");
    }
    fprintf(out, "};\nconst size_t %s_SIZE = %d;\n", cfg->symbol_plain, final_count);

    fclose(out);
    free(final_entries);
    return final_count;
}

/* -------------------------------------------------------------------------
 * Huffman Compressed Dictionary Generator
 * ------------------------------------------------------------------------- */
typedef struct huff_node_s {
    int freq;
    int char_code;
    struct huff_node_s* left;
    struct huff_node_s* right;
} huff_tree_node_t;

typedef struct {
    char bits[64];
    int len;
} huff_code_t;

typedef struct {
    uint16_t left;
    uint16_t right;
    char comment[32];
} c_node_t;

static int compare_nodes(const void* a, const void* b) {
    const huff_tree_node_t* na = *(const huff_tree_node_t**)a;
    const huff_tree_node_t* nb = *(const huff_tree_node_t**)b;
    return na->freq - nb->freq;
}

static void generate_codes(huff_tree_node_t* root, char* current_code, int depth, huff_code_t* codes) {
    if (!root) return;
    if (root->char_code != -1) {
        current_code[depth] = '\0';
        strcpy(codes[(unsigned char)root->char_code].bits, current_code);
        codes[(unsigned char)root->char_code].len = depth;
        return;
    }
    current_code[depth] = '0';
    generate_codes(root->left, current_code, depth + 1, codes);
    current_code[depth] = '1';
    generate_codes(root->right, current_code, depth + 1, codes);
}

static int serialize_tree(huff_tree_node_t* node, c_node_t* c_nodes, int* count) {
    if (!node) return 0;
    if (node->char_code != -1) {
        int idx = (*count)++;
        c_nodes[idx].left = 0xFFFF;
        c_nodes[idx].right = (uint16_t)(unsigned char)node->char_code;
        snprintf(c_nodes[idx].comment, sizeof(c_nodes[idx].comment), "LEAF: '%c'", (char)node->char_code);
        return idx;
    }
    int idx = (*count)++;
    int left_idx = serialize_tree(node->left, c_nodes, count);
    int right_idx = serialize_tree(node->right, c_nodes, count);
    c_nodes[idx].left = (uint16_t)left_idx;
    c_nodes[idx].right = (uint16_t)right_idx;
    snprintf(c_nodes[idx].comment, sizeof(c_nodes[idx].comment), "INTERNAL");
    return idx;
}

static int get_shared_prefix_len(const char* s1, const char* s2) {
    int len = 0;
    while (s1[len] && s2[len] && s1[len] == s2[len]) len++;
    return len;
}

static int generate_huffman(const locale_config_t* cfg, dict_entry_raw_t* raw, int raw_count, const char* out_path) {
    qsort(raw, raw_count, sizeof(dict_entry_raw_t), compare_words_only);

    /* Frequency count of phoneme characters */
    int freqs[256] = {0};
    for (int i = 0; i < raw_count; i++) {
        const char* p = raw[i].phonemes;
        while (*p) freqs[(unsigned char)*p++]++;
    }

    /* Build Huffman forest */
    huff_tree_node_t* heap[256];
    int heap_size = 0;
    for (int i = 0; i < 256; i++) {
        if (freqs[i] > 0) {
            huff_tree_node_t* node = (huff_tree_node_t*)malloc(sizeof(huff_tree_node_t));
            node->freq = freqs[i];
            node->char_code = i;
            node->left = NULL;
            node->right = NULL;
            heap[heap_size++] = node;
        }
    }

    if (heap_size == 0) return -1;

    while (heap_size > 1) {
        qsort(heap, heap_size, sizeof(huff_tree_node_t*), compare_nodes);
        huff_tree_node_t* n1 = heap[0];
        huff_tree_node_t* n2 = heap[1];

        huff_tree_node_t* parent = (huff_tree_node_t*)malloc(sizeof(huff_tree_node_t));
        parent->freq = n1->freq + n2->freq;
        parent->char_code = -1;
        parent->left = n1;
        parent->right = n2;

        heap[0] = parent;
        for (int k = 1; k < heap_size - 1; k++) heap[k] = heap[k + 1];
        heap_size--;
    }

    huff_tree_node_t* root = heap[0];
    huff_code_t codes[256] = {0};
    char temp_code[64] = {0};
    generate_codes(root, temp_code, 0, codes);

    c_node_t c_nodes[512];
    int c_node_count = 0;
    serialize_tree(root, c_nodes, &c_node_count);

    /* Front-coding + Huffman compression */
    size_t blob_cap = 512 * 1024;
    uint8_t* blob = (uint8_t*)malloc(blob_cap);
    size_t blob_len = 0;

    typedef struct {
        char first_word[MAX_WORD_LEN];
        uint32_t offset;
    } block_idx_entry_t;

    block_idx_entry_t block_index[MAX_ENTRIES / BLOCK_SIZE + 16];
    int block_index_count = 0;

    char prev_word[MAX_WORD_LEN] = "";
    for (int i = 0; i < raw_count; i++) {
        if (i % BLOCK_SIZE == 0) {
            strcpy(block_index[block_index_count].first_word, raw[i].word);
            block_index[block_index_count].offset = (uint32_t)blob_len;
            block_index_count++;
            prev_word[0] = '\0';
        }

        int shared = get_shared_prefix_len(raw[i].word, prev_word);
        const char* suffix = raw[i].word + shared;
        size_t suffix_len = strlen(suffix);

        /* Encode phonemes into bit string */
        char bit_string[1024] = {0};
        const char* ph = raw[i].phonemes;
        while (*ph) {
            strcat(bit_string, codes[(unsigned char)*ph].bits);
            ph++;
        }
        int huff_bits_len = (int)strlen(bit_string);

        /* Pad to byte boundary */
        int padding = (8 - (huff_bits_len % 8)) % 8;
        for (int pad = 0; pad < padding; pad++) strcat(bit_string, "1");

        /* Write entry */
        blob[blob_len++] = (uint8_t)shared;
        blob[blob_len++] = (uint8_t)suffix_len;
        memcpy(blob + blob_len, suffix, suffix_len);
        blob_len += suffix_len;

        blob[blob_len++] = (uint8_t)(huff_bits_len & 0xFF);
        blob[blob_len++] = (uint8_t)((huff_bits_len >> 8) & 0xFF);

        for (int b = 0; b < (int)strlen(bit_string); b += 8) {
            uint8_t byte_val = 0;
            for (int bit = 0; bit < 8; bit++) {
                if (bit_string[b + bit] == '1') byte_val |= (1 << (7 - bit));
            }
            blob[blob_len++] = byte_val;
        }

        strcpy(prev_word, raw[i].word);
    }

    FILE* out = fopen(out_path, "w");
    if (!out) {
        free(blob);
        return -1;
    }

    fprintf(out, "/**\n * @file %s\n * @brief Compressed %s dictionary (Front-Coded + Huffman)\n */\n\n",
            out_path, cfg->description);
    fprintf(out, "#include \"espure_internal.h\"\n#include <stdint.h>\n#include <stddef.h>\n\n");
    fprintf(out, "#ifdef CONFIG_ESPURE_DICT_HUFFMAN_TRIE\n\n");
    fprintf(out, "typedef struct { uint16_t left; uint16_t right; } huff_node_t;\n\n");
    fprintf(out, "static const huff_node_t DICT_HUFFMAN_TREE[] = {\n");
    for (int k = 0; k < c_node_count; k++) {
        fprintf(out, "    {0x%04X, 0x%04X}%s /* %s */\n",
                c_nodes[k].left, c_nodes[k].right,
                (k + 1 < c_node_count) ? "," : "", c_nodes[k].comment);
    }
    fprintf(out, "};\n\n");

    fprintf(out, "typedef struct { const char* first_word; uint32_t offset; } block_index_t;\n\n");
    fprintf(out, "static const block_index_t DICT_BLOCK_INDEX[%d] = {\n", block_index_count);
    for (int k = 0; k < block_index_count; k++) {
        fprintf(out, "    {");
        write_c_string(out, block_index[k].first_word);
        fprintf(out, ", %u},\n", block_index[k].offset);
    }
    fprintf(out, "};\n\n");

    fprintf(out, "static const uint8_t DICT_BLOB_DATA[%zu] = {\n    ", blob_len);
    for (size_t k = 0; k < blob_len; k++) {
        fprintf(out, "0x%02X, ", blob[k]);
        if ((k + 1) % 16 == 0) fprintf(out, "\n    ");
    }
    fprintf(out, "\n};\n\n");

    /* Emit standard embedded lookup function */
    fprintf(out,
        "espure_err_t espure_dict_compressed_lookup(const char* word, char* out_phonemes, size_t out_size) {\n"
        "    if (!word || !out_phonemes || out_size == 0) return ESPURE_ERR_INVALID_ARG;\n"
        "    size_t num_blocks = sizeof(DICT_BLOCK_INDEX) / sizeof(DICT_BLOCK_INDEX[0]);\n"
        "    if (num_blocks == 0) return ESPURE_ERR_NOT_FOUND;\n"
        "    int left = 0, right = (int)num_blocks - 1, block_idx = 0;\n"
        "    while (left <= right) {\n"
        "        int mid = left + (right - left) / 2;\n"
        "        int cmp = strcmp(word, DICT_BLOCK_INDEX[mid].first_word);\n"
        "        if (cmp == 0) { block_idx = mid; break; }\n"
        "        else if (cmp < 0) { right = mid - 1; }\n"
        "        else { block_idx = mid; left = mid + 1; }\n"
        "    }\n"
        "    uint32_t offset = DICT_BLOCK_INDEX[block_idx].offset;\n"
        "    uint32_t end_offset = (block_idx + 1 < (int)num_blocks) ? DICT_BLOCK_INDEX[block_idx + 1].offset : (uint32_t)sizeof(DICT_BLOB_DATA);\n"
        "    char current_word[128] = {0};\n"
        "    while (offset < end_offset) {\n"
        "        uint8_t shared = DICT_BLOB_DATA[offset++];\n"
        "        uint8_t suffix_len = DICT_BLOB_DATA[offset++];\n"
        "        for (int i = 0; i < suffix_len; i++) current_word[shared + i] = DICT_BLOB_DATA[offset++];\n"
        "        current_word[shared + suffix_len] = '\\0';\n"
        "        uint16_t huff_bits_len = DICT_BLOB_DATA[offset++];\n"
        "        huff_bits_len |= (uint16_t)DICT_BLOB_DATA[offset++] << 8;\n"
        "        uint32_t huff_bytes_len = (huff_bits_len + 7) / 8;\n"
        "        if (strcmp(word, current_word) == 0) {\n"
        "            size_t out_pos = 0;\n"
        "            uint16_t node = 0;\n"
        "            for (int bit = 0; bit < huff_bits_len; bit++) {\n"
        "                uint8_t byte_val = DICT_BLOB_DATA[offset + (bit / 8)];\n"
        "                int bit_val = (byte_val >> (7 - (bit % 8))) & 1;\n"
        "                node = (bit_val == 0) ? DICT_HUFFMAN_TREE[node].left : DICT_HUFFMAN_TREE[node].right;\n"
        "                if (DICT_HUFFMAN_TREE[node].left == 0xFFFF) {\n"
        "                    if (out_pos + 1 >= out_size) { out_phonemes[0] = '\\0'; return ESPURE_ERR_BUFFER_TOO_SMALL; }\n"
        "                    out_phonemes[out_pos++] = (char)DICT_HUFFMAN_TREE[node].right;\n"
        "                    node = 0;\n"
        "                }\n"
        "            }\n"
        "            out_phonemes[out_pos] = '\\0';\n"
        "            return ESPURE_OK;\n"
        "        }\n"
        "        offset += huff_bytes_len;\n"
        "    }\n"
        "    return ESPURE_ERR_NOT_FOUND;\n"
        "}\n"
        "#endif // CONFIG_ESPURE_DICT_HUFFMAN_TRIE\n"
    );

    fclose(out);
    free(blob);
    return raw_count;
}

/* -------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */
int main(int argc, char** argv) {
    const char* locale = "de-DE";
    const char* format = "plain";
    const char* custom_input = NULL;
    const char* custom_output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--locale") == 0 && i + 1 < argc) locale = argv[++i];
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) format = argv[++i];
        else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) custom_input = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) custom_output = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: build_dict --locale <de-DE|en-GB> --format <plain|huffman> [--input <file.json>] [--output <out.c>]\n");
            return 0;
        }
    }

    const locale_config_t* cfg = NULL;
    for (size_t i = 0; i < sizeof(CONFIGS) / sizeof(CONFIGS[0]); i++) {
        if (strcmp(CONFIGS[i].locale, locale) == 0) {
            cfg = &CONFIGS[i];
            break;
        }
    }
    if (!cfg) {
        fprintf(stderr, "Error: Unsupported locale '%s'\n", locale);
        return 1;
    }

    char in_path[256];
    if (custom_input) {
        strncpy(in_path, custom_input, sizeof(in_path) - 1);
    } else {
        snprintf(in_path, sizeof(in_path), ".data/dictsource/%s", cfg->input_file);
        FILE* test_f = fopen(in_path, "r");
        if (!test_f) {
            snprintf(in_path, sizeof(in_path), "data/dictsource/%s", cfg->input_file);
        } else {
            fclose(test_f);
        }
    }

    char out_path[256];
    if (custom_output) {
        strncpy(out_path, custom_output, sizeof(out_path) - 1);
    } else {
        strncpy(out_path, (strcmp(format, "plain") == 0) ? cfg->output_plain : cfg->output_huffman, sizeof(out_path) - 1);
    }

    printf("build_dict (Native C Host Tool):\n");
    printf("  Locale: %s\n", locale);
    printf("  Format: %s\n", format);
    printf("  Input:  %s\n", in_path);
    printf("  Output: %s\n", out_path);

    FILE* in = fopen(in_path, "rb");
    if (!in) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", in_path);
        return 1;
    }
    fseek(in, 0, SEEK_END);
    long fsize = ftell(in);
    fseek(in, 0, SEEK_SET);

    char* json_buf = (char*)malloc(fsize + 1);
    if (!json_buf) {
        fclose(in);
        fprintf(stderr, "Error: Out of memory reading %ld bytes\n", fsize);
        return 1;
    }
    size_t read_bytes = fread(json_buf, 1, fsize, in);
    json_buf[read_bytes] = '\0';
    fclose(in);

    dict_entry_raw_t* raw_entries = (dict_entry_raw_t*)malloc(sizeof(dict_entry_raw_t) * MAX_ENTRIES);
    if (!raw_entries) {
        free(json_buf);
        fprintf(stderr, "Error: Out of memory allocating entry table\n");
        return 1;
    }

    int count = parse_json_dict(json_buf, raw_entries, MAX_ENTRIES);
    free(json_buf);
    if (count <= 0) {
        free(raw_entries);
        fprintf(stderr, "Error: Failed to parse JSON exception dictionary\n");
        return 1;
    }
    printf("  Parsed: %d raw exceptions\n", count);

    int res = 0;
    if (strcmp(format, "plain") == 0) {
        res = generate_plain(cfg, raw_entries, count, out_path);
    } else if (strcmp(format, "huffman") == 0) {
        res = generate_huffman(cfg, raw_entries, count, out_path);
    } else {
        fprintf(stderr, "Error: Unknown format '%s'\n", format);
        res = -1;
    }

    free(raw_entries);
    if (res < 0) {
        fprintf(stderr, "Error generating dictionary file\n");
        return 1;
    }

    printf("  Success: generated %d entries in %s\n", res, out_path);
    return 0;
}
