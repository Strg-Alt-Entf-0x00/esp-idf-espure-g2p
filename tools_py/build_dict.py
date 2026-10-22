#!/usr/bin/env python3
"""
Unified Dictionary Builder for ESPure G2P

Generates C dictionary files from JSON exception sources in multiple formats.
Supports both plain binary-search dictionaries and Huffman-compressed variants.

Usage:
    python build_dict.py --locale de-DE --format plain
    python build_dict.py --locale de-DE --format huffman
    python build_dict.py --locale en-GB --format plain

Author: ESPure G2P Project
License: See repository LICENSE file
"""

import argparse
import json
import pathlib
import struct
import sys
from collections import Counter
import heapq
from typing import Dict, List, Tuple, Optional


# =============================================================================
# Configuration
# =============================================================================

ROOT = pathlib.Path(__file__).parent.parent

LOCALE_CONFIG = {
    "de-DE": {
        "data_key": "de_DE",
        "input_file": "de_DE_espeak_exceptions.json",
        "output_plain": "lang/de_DE/de_DE_dict.c",
        "output_huffman": "lang/de_DE/de_DE_dict_compressed.c",
        "symbol_plain": "DICT_DE_DE",
        "description": "German exception",
        "normalizer": "german",
    },
    "en-GB": {
        "data_key": "en_GB",
        "input_file": "en_GB_pilot_exceptions.json",
        "output_plain": "lang/en_GB/en_GB_dict.c",
        "output_huffman": "lang/en_GB/en_GB_dict_compressed.c",
        "symbol_plain": "DICT_EN_GB",
        "description": "en-GB architecture-pilot",
        "normalizer": "english",
    },
}


# =============================================================================
# Validation
# =============================================================================

def validate_exceptions(exceptions: Dict[str, str]) -> None:
    """Validate exception dictionary structure and content."""
    if not isinstance(exceptions, dict):
        raise ValueError("Exception dictionary must be a JSON object")

    for word, pronunciation in exceptions.items():
        if not isinstance(word, str) or not isinstance(pronunciation, str):
            raise ValueError(
                "Exception dictionary keys and pronunciations must be strings"
            )
        if not word or not pronunciation or "\0" in word or "\0" in pronunciation:
            raise ValueError(
                "Exception dictionary entries require non-empty keys and "
                "pronunciations without NUL characters"
            )
        if len(word.encode("utf-8")) >= 128:
            raise ValueError(
                f"Dictionary key exceeds the runtime word limit: "
                f"{json.dumps(word, ensure_ascii=True)}"
            )


# =============================================================================
# Normalization
# =============================================================================

def normalize_german_word(word: str) -> str:
    """Normalize German word to lowercase with German-specific rules."""
    normalized = []
    for char in word:
        codepoint = ord(char)
        # A-Z, À-Ö, Ø-Þ → lowercase
        if "A" <= char <= "Z" or 0xC0 <= codepoint <= 0xD6 or 0xD8 <= codepoint <= 0xDE:
            normalized.append(chr(codepoint + 0x20))
        # Capital ẞ → ß
        elif char == "\u1e9e":
            normalized.append("\u00df")
        else:
            normalized.append(char)
    return "".join(normalized)


def normalize_english_word(word: str) -> str:
    """Normalize English word to lowercase."""
    return word.lower()


def get_normalizer(locale: str):
    """Get the appropriate word normalizer for a locale."""
    normalizer_name = LOCALE_CONFIG[locale]["normalizer"]
    if normalizer_name == "german":
        return normalize_german_word
    elif normalizer_name == "english":
        return normalize_english_word
    else:
        raise ValueError(f"Unknown normalizer: {normalizer_name}")


# =============================================================================
# C Code Generation Utilities
# =============================================================================

def c_string_literal(value: str) -> str:
    """Escape a string for C string literal."""
    escaped = []
    for char in value:
        if char == "\\":
            escaped.append("\\\\")
        elif char == '"':
            escaped.append('\\"')
        elif char == "\n":
            escaped.append("\\n")
        elif char == "\r":
            escaped.append("\\r")
        elif char == "\t":
            escaped.append("\\t")
        elif ord(char) < 0x20 or ord(char) == 0x7F:
            escaped.append(f"\\{ord(char):03o}")
        else:
            escaped.append(char)
    return '"' + "".join(escaped) + '"'


# =============================================================================
# JSON Loading with Duplicate Detection
# =============================================================================

def load_unique_json_object(pairs: List[Tuple[str, str]]) -> Dict[str, str]:
    """Load JSON object with duplicate key detection."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(
                f"Duplicate exact dictionary key: {json.dumps(key, ensure_ascii=True)}"
            )
        result[key] = value
    return result


# =============================================================================
# Plain Dictionary Format
# =============================================================================

def process_exceptions_for_plain(
    exceptions: Dict[str, str], normalizer
) -> Dict[str, str]:
    """
    Process exceptions with case-aware grouping.
    
    Groups entries by normalized form. If all variants have the same pronunciation,
    stores only the normalized form. Otherwise, keeps distinct spellings.
    """
    groups = {}
    for word, pronunciation in exceptions.items():
        normalized = normalizer(word)
        groups.setdefault(normalized, []).append((word, pronunciation))

    entries_by_word = {}
    for normalized_word, variants in groups.items():
        pronunciations = {pronunciation for _, pronunciation in variants}
        if len(pronunciations) == 1:
            # All variants have same pronunciation - use normalized form
            entries_by_word[normalized_word] = variants[0][1]
        else:
            # Different pronunciations - keep all distinct spellings
            entries_by_word.update(variants)

    return entries_by_word


def generate_plain_dictionary(
    exceptions: Dict[str, str], locale: str
) -> Tuple[str, int]:
    """Generate plain C dictionary with binary search support."""
    validate_exceptions(exceptions)
    
    config = LOCALE_CONFIG[locale]
    normalizer = get_normalizer(locale)
    
    # Process and group entries
    entries_by_word = process_exceptions_for_plain(exceptions, normalizer)
    
    # Sort for binary search (normalized key, then exact spelling)
    words = sorted(entries_by_word, key=lambda w: (normalizer(w), w))
    
    # Generate C entries
    entries = [
        f"    {{{c_string_literal(word)}, {c_string_literal(entries_by_word[word])}, 0}}"
        for word in words
    ]
    
    # Build C source
    c_code = (
        '#include "espure_internal.h"\n\n'
        "/**\n"
        f" * @brief Generated {config['description']} dictionary.\n"
        f" * Generated from data/dictsource/{config['input_file']}.\n"
        " */\n"
        f"extern const espure_dict_entry_t {config['symbol_plain']}[];\n"
        f"extern const size_t {config['symbol_plain']}_SIZE;\n\n"
        f"const espure_dict_entry_t {config['symbol_plain']}[] = {{\n"
        + ",\n".join(entries)
        + f"\n}};\nconst size_t {config['symbol_plain']}_SIZE = {len(words)};\n"
    )
    
    return c_code, len(words)


# =============================================================================
# Huffman Compression
# =============================================================================

class HuffmanNode:
    """Node in Huffman tree."""
    
    def __init__(self, char: Optional[str], freq: int):
        self.char = char
        self.freq = freq
        self.left: Optional['HuffmanNode'] = None
        self.right: Optional['HuffmanNode'] = None

    def __lt__(self, other: 'HuffmanNode') -> bool:
        return self.freq < other.freq


def build_huffman_tree(
    phoneme_strings: List[str]
) -> Tuple[Optional[HuffmanNode], Dict[str, str]]:
    """Build Huffman tree from phoneme strings and return codes."""
    # Count character frequencies
    freqs = Counter()
    for p in phoneme_strings:
        freqs.update(p)
    
    # Build priority queue
    heap = [HuffmanNode(ch, f) for ch, f in freqs.items()]
    heapq.heapify(heap)
    
    if len(heap) == 0:
        return None, {}
    if len(heap) == 1:
        node = heap[0]
        return node, {node.char: '0'}
    
    # Build tree
    while len(heap) > 1:
        left = heapq.heappop(heap)
        right = heapq.heappop(heap)
        merged = HuffmanNode(None, left.freq + right.freq)
        merged.left = left
        merged.right = right
        heapq.heappush(heap, merged)
    
    root = heap[0]
    
    # Generate codes
    codes = {}
    
    def build_codes(node: HuffmanNode, current_code: str):
        if node.char is not None:
            codes[node.char] = current_code
        else:
            build_codes(node.left, current_code + '0')
            build_codes(node.right, current_code + '1')
    
    build_codes(root, '')
    return root, codes


def serialize_huffman_tree(root: HuffmanNode) -> List[str]:
    """Serialize Huffman tree to C array format."""
    c_nodes = []
    
    def serialize_node(node: HuffmanNode) -> int:
        if node.char is not None:
            # Leaf: left=0xFFFF (marker), right=char code
            c_nodes.append(f"    {{0xFFFF, {ord(node.char)}}} /* LEAF: '{node.char}' */")
            return len(c_nodes) - 1
        else:
            # Internal node
            idx = len(c_nodes)
            c_nodes.append("    {0, 0} /* INTERNAL (placeholder) */")
            left_idx = serialize_node(node.left)
            right_idx = serialize_node(node.right)
            c_nodes[idx] = f"    {{{left_idx}, {right_idx}}} /* INTERNAL */"
            return idx
    
    serialize_node(root)
    return c_nodes


def get_shared_prefix_len(s1: str, s2: str) -> int:
    """Get length of shared prefix between two strings."""
    m = min(len(s1), len(s2))
    for i in range(m):
        if s1[i] != s2[i]:
            return i
    return m


def generate_huffman_dictionary(
    exceptions: Dict[str, str], locale: str
) -> Tuple[str, int, int]:
    """
    Generate Huffman-compressed C dictionary.
    
    Returns: (c_code, entry_count, compressed_size_bytes)
    """
    validate_exceptions(exceptions)
    
    config = LOCALE_CONFIG[locale]
    normalizer = get_normalizer(locale)
    
    # Sort words
    words = sorted(exceptions.keys())
    
    # Build Huffman tree for phonemes
    root, huff_codes = build_huffman_tree(list(exceptions.values()))
    c_huff_nodes = serialize_huffman_tree(root)
    
    # Front-coding with Huffman compression
    BLOCK_SIZE = 128
    block_data = bytearray()
    block_index = []  # (first_word, byte_offset)
    
    prev_word = ""
    for i, word in enumerate(words):
        if i % BLOCK_SIZE == 0:
            block_index.append((word, len(block_data)))
            prev_word = ""
        
        # Front coding: store shared prefix length + suffix
        shared = get_shared_prefix_len(word, prev_word)
        suffix = word[shared:].encode('utf-8')
        
        # Huffman encode phonemes
        phonemes = exceptions[word]
        bit_string = ''.join(huff_codes[ch] for ch in phonemes)
        
        # Pad to byte boundary
        padding = (8 - (len(bit_string) % 8)) % 8
        bit_string += '1' * padding
        
        huff_bytes = bytearray()
        for b in range(0, len(bit_string), 8):
            byte_val = int(bit_string[b:b+8], 2)
            huff_bytes.append(byte_val)
        
        # Write entry: [shared_len][suffix_len][suffix][huff_bits_len_LE][huff_bytes]
        block_data.append(shared)
        block_data.append(len(suffix))
        block_data.extend(suffix)
        
        huff_bits_len = len(''.join(huff_codes[ch] for ch in phonemes))
        huff_bits_len = min(huff_bits_len, 65535)
        
        block_data.append(huff_bits_len & 0xFF)
        block_data.append((huff_bits_len >> 8) & 0xFF)
        block_data.extend(huff_bytes)
        
        prev_word = word
    
    # Generate C code
    c_code = f"""/**
 * @file {pathlib.Path(config['output_huffman']).name}
 * @brief Compressed {config['description']} dictionary (Front-Coded + Huffman)
 * Generated from data/dictsource/{config['input_file']}.
 */

#include "espure_internal.h"
#include <stdint.h>
#include <stddef.h>

#ifdef CONFIG_ESPURE_DICT_HUFFMAN_TRIE

typedef struct {{ uint16_t left; uint16_t right; }} huff_node_t;

static const huff_node_t DICT_HUFFMAN_TREE[] = {{
"""
    c_code += ",\n".join(c_huff_nodes)
    c_code += "\n};\n\n"
    
    # Block index
    c_code += "typedef struct { const char* first_word; uint32_t offset; } block_index_t;\n\n"
    c_code += f"static const block_index_t DICT_BLOCK_INDEX[{len(block_index)}] = {{\n"
    for w, off in block_index:
        c_code += f'    {{{c_string_literal(w)}, {off}}},\n'
    c_code += "};\n\n"
    
    # Blob data
    c_code += f"static const uint8_t DICT_BLOB_DATA[{len(block_data)}] = {{\n    "
    for i, b in enumerate(block_data):
        c_code += f"0x{b:02X}, "
        if (i + 1) % 16 == 0:
            c_code += "\n    "
    c_code += "\n};\n\n"
    
    # Lookup function
    c_code += """
// Search function to find a word in the compressed blob
espure_err_t espure_dict_compressed_lookup(const char* word, char* out_phonemes, size_t out_size) {
    if (!word || !out_phonemes || out_size == 0) {
        return ESPURE_ERR_INVALID_ARG;
    }

    size_t num_blocks = sizeof(DICT_BLOCK_INDEX) / sizeof(DICT_BLOCK_INDEX[0]);
    if (num_blocks == 0) return ESPURE_ERR_NOT_FOUND;
    
    // Binary search for the block
    int left = 0;
    int right = num_blocks - 1;
    int block_idx = 0;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(word, DICT_BLOCK_INDEX[mid].first_word);
        
        if (cmp == 0) {
            block_idx = mid;
            break;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            block_idx = mid;
            left = mid + 1;
        }
    }
    
    // Sequential scan within the block
    uint32_t offset = DICT_BLOCK_INDEX[block_idx].offset;
    uint32_t end_offset = (block_idx + 1 < num_blocks) ? DICT_BLOCK_INDEX[block_idx + 1].offset : sizeof(DICT_BLOB_DATA);
    
    char current_word[128] = {0};
    
    while (offset < end_offset) {
        uint8_t shared = DICT_BLOB_DATA[offset++];
        uint8_t suffix_len = DICT_BLOB_DATA[offset++];
        
        for (int i = 0; i < suffix_len; i++) {
            current_word[shared + i] = DICT_BLOB_DATA[offset++];
        }
        current_word[shared + suffix_len] = '\\0';
        
        uint16_t huff_bits_len = DICT_BLOB_DATA[offset++];
        huff_bits_len |= (uint16_t)DICT_BLOB_DATA[offset++] << 8;
        uint32_t huff_bytes_len = (huff_bits_len + 7) / 8;
        
        if (strcmp(word, current_word) == 0) {
            // Found! Decode Huffman
            size_t out_pos = 0;
            uint16_t node = 0; // root is always 0
            
            for (int bit = 0; bit < huff_bits_len; bit++) {
                uint8_t byte_val = DICT_BLOB_DATA[offset + (bit / 8)];
                int bit_val = (byte_val >> (7 - (bit % 8))) & 1;
                
                if (bit_val == 0) {
                    node = DICT_HUFFMAN_TREE[node].left;
                } else {
                    node = DICT_HUFFMAN_TREE[node].right;
                }
                
                if (DICT_HUFFMAN_TREE[node].left == 0xFFFF) { // Leaf
                    if (out_pos + 1 >= out_size) {
                        out_phonemes[0] = '\\0';
                        return ESPURE_ERR_BUFFER_TOO_SMALL;
                    }
                    out_phonemes[out_pos++] = (char)DICT_HUFFMAN_TREE[node].right;
                    node = 0; // reset to root
                }
            }
            out_phonemes[out_pos] = '\\0';
            return ESPURE_OK;
        }
        
        // Skip huffman bytes and continue
        offset += huff_bytes_len;
    }
    
    return ESPURE_ERR_NOT_FOUND;
}
#endif // CONFIG_ESPURE_DICT_HUFFMAN_TRIE
"""
    
    return c_code, len(words), len(block_data)


# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate C dictionary files from JSON exception sources",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --locale de-DE --format plain
  %(prog)s --locale de-DE --format huffman
  %(prog)s --locale en-GB --format plain

Supported locales: de-DE, en-GB
Supported formats: plain, huffman
        """,
    )
    parser.add_argument(
        "--locale",
        choices=list(LOCALE_CONFIG.keys()),
        required=True,
        help="Target locale/language",
    )
    parser.add_argument(
        "--format",
        choices=["plain", "huffman"],
        default="plain",
        help="Dictionary format (default: plain)",
    )
    
    args = parser.parse_args()
    
    # Get configuration
    config = LOCALE_CONFIG[args.locale]
    input_path = ROOT / "data" / "dictsource" / config["input_file"]
    
    if args.format == "plain":
        output_path = ROOT / config["output_plain"]
    else:
        output_path = ROOT / config["output_huffman"]
    
    # Load and validate input
    print(f"Loading {input_path.name}...")
    try:
        exceptions = json.loads(
            input_path.read_text(encoding="utf-8"),
            object_pairs_hook=load_unique_json_object,
        )
    except (OSError, json.JSONDecodeError, ValueError) as e:
        print(f"Error loading input: {e}", file=sys.stderr)
        return 1
    
    # Generate output
    print(f"Generating {args.format} format dictionary for {args.locale}...")
    try:
        if args.format == "plain":
            c_code, entry_count = generate_plain_dictionary(exceptions, args.locale)
            print(f"Generated {output_path.name} with {entry_count} entries.")
        else:
            c_code, entry_count, blob_size = generate_huffman_dictionary(
                exceptions, args.locale
            )
            print(
                f"Generated {output_path.name} with {entry_count} entries "
                f"(blob: {blob_size} bytes, compression: "
                f"{(1 - blob_size/(entry_count*50)):.1%})."
            )
    except (ValueError, KeyError) as e:
        print(f"Error generating dictionary: {e}", file=sys.stderr)
        return 1
    
    # Write output
    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(c_code, encoding="utf-8")
        print(f"Successfully wrote {output_path}")
    except OSError as e:
        print(f"Error writing output: {e}", file=sys.stderr)
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
