#include "espure_internal.h"
#include "espure_phoneme_program.h"
#include <string.h>

#define STRESS_IS_PRIMARY   4
#define STRESS_IS_SECONDARY 3
#define STRESS_IS_UNSTRESSED 1
#define STRESS_IS_DIMINISHED 0
#define STRESS_IS_NOT_STRESSED -1

void espure_assign_word_stress(const espure_translator_t* tr, espure_phlist_entry_t* plist, size_t plist_len) {
    if (plist_len == 0 || !tr) return;

    int vowel_indices[32]; 
    int vowel_stress[32];
    int vowel_count = 1;   
    
    for (int i = 0; i < 32; i++) {
        vowel_stress[i] = STRESS_IS_NOT_STRESSED;
    }
    
    int max_stress = STRESS_IS_NOT_STRESSED;
    
    for (size_t i = 0; i < plist_len; i++) {
        if (plist[i].ph->type == PH_VOWEL) {
            if (vowel_count < 32) {
                vowel_indices[vowel_count] = i;
                int current_stress = plist[i].stresslevel;
                if (current_stress == 0) {
                    current_stress = STRESS_IS_UNSTRESSED;
                } else if (current_stress == STRESS_PRIMARY) {
                    current_stress = STRESS_IS_PRIMARY;
                } else if (current_stress == STRESS_SECONDARY) {
                    current_stress = STRESS_IS_SECONDARY;
                } else if (current_stress == STRESS_DIMINISHED) {
                    current_stress = STRESS_IS_DIMINISHED;
                }
                
                vowel_stress[vowel_count] = current_stress;
                if (current_stress > max_stress) {
                    max_stress = current_stress;
                }
                
                vowel_count++;
            }
        }
    }
    
    vowel_stress[vowel_count] = STRESS_IS_UNSTRESSED; 
    
    if (vowel_count <= 1) return;

    int stress = (max_stress < STRESS_IS_PRIMARY) ? STRESS_IS_PRIMARY : STRESS_IS_SECONDARY;
    
    for (int v = 1; v < vowel_count; v++) {
        if (vowel_stress[v] < STRESS_IS_DIMINISHED) {
            if ((vowel_stress[v - 1] <= STRESS_IS_UNSTRESSED) &&
                ((vowel_stress[v + 1] <= STRESS_IS_UNSTRESSED) || 
                 (stress == STRESS_IS_PRIMARY && vowel_stress[v + 1] <= STRESS_IS_NOT_STRESSED))) {
                
                vowel_stress[v] = stress;
                stress = STRESS_IS_SECONDARY;
            }
        }
    }
    
    for (int v = 1; v < vowel_count; v++) {
        int v_stress = vowel_stress[v];
        if (v_stress <= STRESS_IS_UNSTRESSED) {
            if (v == 1 || v == vowel_count - 1) {
                v_stress = STRESS_IS_UNSTRESSED;
            } else if (v == vowel_count - 2 && vowel_stress[vowel_count - 1] <= STRESS_IS_UNSTRESSED) {
                v_stress = STRESS_IS_UNSTRESSED;
            } else {
                v_stress = STRESS_IS_DIMINISHED;
            }
            vowel_stress[v] = v_stress;
        }
        
        int plist_idx = vowel_indices[v];
        if (v_stress == STRESS_IS_PRIMARY) {
            plist[plist_idx].stresslevel = STRESS_PRIMARY;
        } else if (v_stress == STRESS_IS_SECONDARY) {
            plist[plist_idx].stresslevel = STRESS_SECONDARY;
        } else if (v_stress == STRESS_IS_UNSTRESSED) {
            plist[plist_idx].stresslevel = STRESS_UNSTRESSED;
        } else if (v_stress == STRESS_IS_DIMINISHED) {
            plist[plist_idx].stresslevel = STRESS_DIMINISHED;
        }
    }
}
