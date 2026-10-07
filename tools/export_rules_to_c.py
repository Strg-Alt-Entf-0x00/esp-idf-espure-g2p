#!/usr/bin/env python3
import sys
import os

from espyak.rule_compiler import RuleSet
import pathlib

RULES_IN = str(pathlib.Path(__file__).parent.parent / 'data' / 'dictsource' / 'de_DE_rules')
OUTPUT_C = str(pathlib.Path(__file__).parent.parent / 'src' / 'de_DE_rules_data.c')
# Output header is not needed because espure_compiled_rules.h is already generic

def export_rules():
    print(f"Compiling {RULES_IN}...")
    rs = RuleSet.compile_file(RULES_IN)
    
    print("Generating C source...")
    with open(OUTPUT_C, "w", encoding="utf-8") as f:
        f.write("#include \"espure_compiled_rules.h\"\n\n")
        
        def write_rule_list(name, rules):
            if not rules:
                return "NULL", 0
                
            f.write(f"// --- {name} ---\n")
            
            for i, r in enumerate(rules):
                prog_hex = ", ".join(f"0x{b:02x}" for b in r.prog)
                if not prog_hex: prog_hex = "0"
                f.write(f"static const uint8_t {name}_prog_{i}[] = {{{prog_hex}}};\n")
                
            f.write(f"static const espure_compiled_rule_t {name}[] = {{\n")
            for i, r in enumerate(rules):
                ph_esc = r.phonemes.replace('\\', '\\\\').replace('"', '\\"')
                m_esc = r.match_str.replace('\\', '\\\\').replace('"', '\\"')
                f.write(f"    {{{name}_prog_{i}, {len(r.prog)}, \"{ph_esc}\", \"{m_esc}\"}},\n")
            f.write("};\n\n")
            return name, len(rules)

        g1_names = {}
        for k, rules in rs.groups1.items():
            name, count = write_rule_list(f"de_DE_g1_{k}", rules)
            g1_names[k] = (name, count)
            
        def_name, def_count = write_rule_list("de_DE_g_default", rs.default)

        f.write("const espure_rule_group_t DE_GROUPS1[256] = {\n")
        for i in range(256):
            if i in g1_names:
                f.write(f"    {{{g1_names[i][0]}, {g1_names[i][1]}}},\n")
            else:
                f.write("    {NULL, 0},\n")
        f.write("};\n\n")
        
        f.write(f"const espure_rule_group_t DE_DEFAULT_GROUP = {{{def_name}, {def_count}}};\n")
        f.write("const espure_group2_t DE_GROUPS2[1] = {{0, {NULL, 0}}};\n")
        f.write("const size_t DE_GROUPS2_COUNT = 0;\n")

    print("Done!")

if __name__ == "__main__":
    export_rules()



