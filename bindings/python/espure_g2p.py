"""
Python wrapper for espure-g2p G2P engine

This provides a pure-Python interface compatible with espyak for training tools.
For production ESP32 use, the native C library is used directly.
"""

import subprocess
import sys
from pathlib import Path
from typing import Optional, List


class G2P:
    """
    Python wrapper for espure-g2p compatible with espyak API.
    
    This implementation uses the compiled host_test executable as a backend
    for phonemization during training/development.
    """
    
    def __init__(self, lang: str = "de"):
        """
        Initialize G2P for a specific language.
        
        Args:
            lang: Language code (e.g., "de" for German)
        """
        # Convert short language codes to full codes
        lang_mapping = {
            "de": "de_DE",
            "en": "en_US",
            "es": "es_ES",
            "fr": "fr_FR",
        }
        self.lang = lang_mapping.get(lang, lang)
        self._cache = {}
        
        # Find the espure library root
        self._lib_root = Path(__file__).parent.parent
        self._host_test_exe = self._find_host_test()
        
    def _find_host_test(self) -> Optional[Path]:
        """Find the host_test executable."""
        candidates = [
            self._lib_root / "host_test" / "x64" / "Debug" / "espure_host_test.exe",
            self._lib_root / "host_test" / "x64" / "Release" / "espure_host_test.exe",
            self._lib_root / "host_test" / "build" / "espure_host_test",
            self._lib_root / "host_test" / "espure_host_test",
        ]
        
        for candidate in candidates:
            if candidate.exists():
                return candidate
        
        print(f"Warning: host_test executable not found. Tried:", file=sys.stderr)
        for candidate in candidates:
            print(f"  - {candidate}", file=sys.stderr)
        print("Phonemization will use fallback mode.", file=sys.stderr)
        return None
    
    def phonemize(self, text: str) -> str:
        """
        Convert text to phonemes (IPA format).
        
        Args:
            text: Input text in the target language
            
        Returns:
            IPA phoneme string
        """
        # Check cache
        if text in self._cache:
            return self._cache[text]
        
        # If no host test available, return placeholder
        if self._host_test_exe is None:
            # Simple fallback: just return the text as-is
            # This allows training pipelines to proceed even without compiled library
            result = text.lower()
            self._cache[text] = result
            return result
        
        # Call host_test via subprocess
        try:
            result = subprocess.run(
                [str(self._host_test_exe), self.lang, text],
                capture_output=True,
                text=True,
                check=True,
                timeout=5
            )
            
            # Parse output (expecting format: "IPA: <phonemes>")
            output = result.stdout.strip()
            if output.startswith("IPA:"):
                phonemes = output[4:].strip()
            else:
                # Fallback: use raw output
                phonemes = output.strip()
            
            self._cache[text] = phonemes
            return phonemes
            
        except subprocess.TimeoutExpired:
            print(f"Warning: Phonemization timeout for '{text}'", file=sys.stderr)
            return text.lower()
        except subprocess.CalledProcessError as e:
            print(f"Warning: Phonemization failed for '{text}': {e}", file=sys.stderr)
            return text.lower()
        except Exception as e:
            print(f"Warning: Unexpected error during phonemization: {e}", file=sys.stderr)
            return text.lower()


# Compatibility exports
__all__ = ["G2P"]
__version__ = "1.0.0"


# Quick test
if __name__ == "__main__":
    g2p = G2P("de")
    test_words = ["Hallo", "Welt", "Bundesrepublik"]
    
    print("Testing espure-g2p Python wrapper:")
    for word in test_words:
        ipa = g2p.phonemize(word)
        print(f"  {word} -> {ipa}")
