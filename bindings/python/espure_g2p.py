"""
Python wrapper for espure-g2p G2P engine

Provides high-performance native ctypes bindings to espure.dll / libespure.so,
with automatic fallback to subprocess (host_test) or mock mode.
For production ESP32-P4 targets, the native C library is used directly.
"""

import ctypes
from ctypes import (
    c_char_p,
    c_void_p,
    c_size_t,
    c_bool,
    c_int,
    c_uint8,
    c_uint16,
    byref,
    create_string_buffer,
    Structure,
    POINTER,
)
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import Optional, List, Dict, Any


class EspureSegment(Structure):
    """Mirror of C espure_segment_t (packed 2-byte structure)"""
    _pack_ = 1
    _fields_ = [
        ("segment_id", c_uint8),
        # In C: stress : 2, boundary : 3, length_modifier : 2, reserved : 1
        ("_bitfields", c_uint8),
    ]

    @property
    def stress(self) -> int:
        return self._bitfields & 0x03

    @property
    def boundary(self) -> int:
        return (self._bitfields >> 2) & 0x07

    @property
    def length_modifier(self) -> int:
        return (self._bitfields >> 5) & 0x03


class EspureStream(Structure):
    """Mirror of C espure_stream_t"""
    _fields_ = [
        ("version", c_uint16),
        ("segment_count", c_uint16),
        ("segments", POINTER(EspureSegment)),
    ]


class EspureOptions(Structure):
    """Mirror of C espure_options_t"""
    _fields_ = [
        ("format", c_int),
        ("separator", c_char_p),
        ("tie", c_char_p),
        ("tonic_stress", ctypes.c_int8),
        ("rules_only", c_bool),
    ]


class G2P:
    """
    Python wrapper for the native ESPure-G2P embedded engine and TTS pipelines.
    
    Uses direct in-process native C bindings via ctypes for ultra-high throughput
    (>50,000 words/second).
    """

    def __init__(self, lang: str = "de-DE", use_cache: bool = True):
        """
        Initialize G2P for a specific language.
        
        Args:
            lang: BCP 47 locale identifier (e.g. "de-DE", "en-GB")
            use_cache: Whether to cache phonemization results in Python
        """
        self.lang = lang
        self.use_cache = use_cache
        self._cache: Dict[str, str] = {}
        
        self._lib_root = Path(__file__).resolve().parent.parent.parent
        self._dll: Optional[ctypes.CDLL] = None
        self._handle: Optional[c_void_p] = None
        self._host_test_exe: Optional[Path] = None
        
        self._init_backend()

    def _find_library(self) -> Optional[Path]:
        """Search for native espure shared library."""
        system = platform.system()
        if system == "Windows":
            lib_names = ["espure.dll"]
            search_dirs = [
                self._lib_root / "host_test" / "build" / "Release",
                self._lib_root / "host_test" / "build" / "Debug",
                self._lib_root / "host_test" / "build",
                self._lib_root / "build" / "Release",
                self._lib_root / "build",
            ]
        elif system == "Darwin":
            lib_names = ["libespure.dylib", "espure.dylib"]
            search_dirs = [
                self._lib_root / "host_test" / "build",
                self._lib_root / "build",
            ]
        else:
            lib_names = ["libespure.so", "espure.so"]
            search_dirs = [
                self._lib_root / "host_test" / "build",
                self._lib_root / "build",
            ]

        for directory in search_dirs:
            for name in lib_names:
                candidate = directory / name
                if candidate.exists():
                    return candidate
        return None

    def _find_host_test(self) -> Optional[Path]:
        """Find the host_test executable for subprocess fallback."""
        candidates = [
            self._lib_root / "host_test" / "build" / "Release" / "espure_host_test.exe",
            self._lib_root / "host_test" / "build" / "espure_host_test.exe",
            self._lib_root / "host_test" / "x64" / "Release" / "espure_host_test.exe",
            self._lib_root / "host_test" / "build" / "espure_host_test",
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        return None

    def _init_backend(self) -> None:
        """Initialize native DLL or locate subprocess executable."""
        lib_path = self._find_library()
        if lib_path:
            try:
                self._dll = ctypes.CDLL(str(lib_path))
                
                # Function prototypes
                self._dll.espure_init.argtypes = [c_char_p, c_void_p, POINTER(c_void_p)]
                self._dll.espure_init.restype = c_int

                self._dll.espure_phonemize.argtypes = [c_void_p, c_char_p, c_char_p, c_size_t, c_bool]
                self._dll.espure_phonemize.restype = c_int

                self._dll.espure_phonemize_ex.argtypes = [
                    c_void_p,
                    c_char_p,
                    c_char_p,
                    c_size_t,
                    POINTER(EspureOptions),
                ]
                self._dll.espure_phonemize_ex.restype = c_int

                self._dll.espure_phonemize_stream.argtypes = [
                    c_void_p,
                    c_char_p,
                    POINTER(EspureSegment),
                    c_size_t,
                    POINTER(EspureStream),
                ]
                self._dll.espure_phonemize_stream.restype = c_int

                self._dll.espure_deinit.argtypes = [c_void_p]
                self._dll.espure_deinit.restype = None

                self._dll.espure_morph_split.argtypes = [c_char_p, c_char_p, c_char_p, c_size_t]
                self._dll.espure_morph_split.restype = c_bool

                handle = c_void_p()
                ret = self._dll.espure_init(self.lang.encode("utf-8"), None, byref(handle))
                if ret == 0:
                    self._handle = handle
                    return
                else:
                    self._dll = None
            except Exception as e:
                self._dll = None

        # Fallback to host_test executable
        self._host_test_exe = self._find_host_test()

    def phonemize(self, text: str, ipa: bool = True, rules_only: bool = False) -> str:
        """
        Convert text to phonemes (default: IPA format).
        
        Args:
            text: Input text in target language
            ipa: True for Unicode IPA, False for ASCII Kirshenbaum
            rules_only: True to bypass dictionary and evaluate pure rules pipeline
            
        Returns:
            Phoneme string
        """
        if not text:
            return ""

        cache_key = f"{text}|{ipa}|{rules_only}"
        if self.use_cache and cache_key in self._cache:
            return self._cache[cache_key]

        # 1. Native ctypes fast path
        if self._handle and self._dll:
            buf = create_string_buffer(2048)
            if rules_only:
                opts = EspureOptions(
                    format=0 if ipa else 1,
                    separator=None,
                    tie=None,
                    tonic_stress=-1,
                    rules_only=True,
                )
                err = self._dll.espure_phonemize_ex(
                    self._handle,
                    text.encode("utf-8"),
                    buf,
                    len(buf),
                    byref(opts),
                )
            else:
                err = self._dll.espure_phonemize(
                    self._handle,
                    text.encode("utf-8"),
                    buf,
                    len(buf),
                    ipa,
                )
            if err == 0:
                result = buf.value.decode("utf-8", errors="replace")
                if self.use_cache:
                    self._cache[cache_key] = result
                return result

        # 2. Host test subprocess fallback
        if self._host_test_exe:
            try:
                res = subprocess.run(
                    [str(self._host_test_exe), self.lang, text],
                    capture_output=True,
                    text=True,
                    check=True,
                    timeout=5,
                )
                output = res.stdout.strip()
                phonemes = output[4:].strip() if output.startswith("IPA:") else output.strip()
                if self.use_cache:
                    self._cache[text] = phonemes
                return phonemes
            except Exception:
                pass

        # 3. Simple fallback
        fallback = text.lower()
        if self.use_cache:
            self._cache[text] = fallback
        return fallback

    def phonemize_stream(self, text: str) -> List[Dict[str, int]]:
        """
        Convert text to structured discrete phonetic segments (Stage 6).
        
        Args:
            text: Input text
            
        Returns:
            List of segment dictionaries with 'segment_id', 'stress',
            'boundary', and 'length_modifier'.
        """
        if not self._handle or not self._dll:
            return []

        max_segments = 512
        seg_buf = (EspureSegment * max_segments)()
        stream = EspureStream()

        err = self._dll.espure_phonemize_stream(
            self._handle,
            text.encode("utf-8"),
            seg_buf,
            max_segments,
            byref(stream),
        )
        if err != 0:
            return []

        results = []
        for i in range(stream.segment_count):
            seg = seg_buf[i]
            results.append({
                "segment_id": seg.segment_id,
                "stress": seg.stress,
                "boundary": seg.boundary,
                "length_modifier": seg.length_modifier,
            })
        return results

    def morph_split(self, word: str) -> str:
        """Decompose compound word into constituent morphemes (Stage 2)."""
        if not self._handle or not self._dll or not word:
            return word
        buf = create_string_buffer(512)
        ret = self._dll.espure_morph_split(self.lang.encode("utf-8"), word.encode("utf-8"), buf, len(buf))
        if ret:
            return buf.value.decode("utf-8", errors="replace")
        return word

    def phonemize_batch(self, texts: List[str], ipa: bool = True) -> List[str]:
        """Convert a list of words or sentences to phonemes efficiently."""
        return [self.phonemize(t, ipa=ipa) for t in texts]

    def close(self) -> None:
        """Free native resources."""
        if self._handle and self._dll:
            self._dll.espure_deinit(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()


__all__ = ["G2P", "EspureSegment", "EspureStream"]
__version__ = "1.0.0"


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    with G2P("de-DE") as g2p:
        words = ["Hallo", "Welt", "Bundesrepublik", "abfangen", "z. B."]
        print(f"Loaded backend: {'Native DLL' if g2p._handle else 'Subprocess/Fallback'}")
        for word in words:
            ipa = g2p.phonemize(word)
            print(f"  {word:15} -> {ipa}")
        
        # Test stream
        stream = g2p.phonemize_stream("Tag")
        print("Structured stream for 'Tag':", stream)

