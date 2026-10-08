"""
Setup script for espure-g2p Python wrapper
"""

from setuptools import setup, find_packages
from pathlib import Path

# Read README
readme_file = Path(__file__).parent.parent / "README.md"
long_description = readme_file.read_text(encoding="utf-8") if readme_file.exists() else ""

setup(
    name="espure-g2p",
    version="1.0.0",
    description="Python wrapper for espure-g2p: State-of-the-art G2P engine for ESP32",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="esp-idf-espure-g2p Contributors",
    url="https://github.com/Strg-Alt-Entf-0x00/esp-idf-espure-g2p",
    packages=find_packages(),
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Multimedia :: Sound/Audio :: Speech",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
    keywords="g2p phonemization esp32 tts speech",
)
