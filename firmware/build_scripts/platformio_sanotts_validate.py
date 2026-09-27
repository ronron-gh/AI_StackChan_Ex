"""Validate the user-supplied SanoTTS library, model, and dictionary."""
Import("env")

import hashlib
from pathlib import Path


ROOT = Path(env.subst("$PROJECT_DIR"))
MODEL = ROOT / "model" / "student_i8.bin"
DICTIONARY = ROOT / "model" / "k1-dict-44000-2mb.bin"
MODEL_SHA256 = "2d2b8543c06b6a749f19c9918de68244409e2bb6ad1d921a90b5c358f96d4d79"
DICTIONARY_SHA256 = "cd1ed65241600b29ced9fde627b1543d93f5f5c0cd5297a4dba42d3f01d8806a"


def require_file(path, expected_hash):
    if not path.is_file():
        raise RuntimeError("SanoTTS required file is missing: " + str(path))
    actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual_hash != expected_hash:
        raise RuntimeError(
            "SanoTTS SHA-256 mismatch: {} (expected {}, got {})".format(
                path, expected_hash, actual_hash
            )
        )


core = ROOT / "lib" / "saanotts_core"
required_core_files = [
    core / "library.json",
    core / "platformio_build.py",
    core / "saanotts.h",
    core / "saanotts_stream.h",
    core / "saan_kanji.h",
]
for required in required_core_files:
    if not required.is_file():
        raise RuntimeError("SanoTTS library is incomplete: " + str(required))

required_scripts = [
    ROOT / "scripts" / "platformio_model.py",
    ROOT / "scripts" / "blob_to_header.py",
    ROOT / "scripts" / "platformio_dictionary.py",
    ROOT / "scripts" / "dictionary_to_header.py",
]
for required in required_scripts:
    if not required.is_file():
        raise RuntimeError("SanoTTS build script is missing: " + str(required))

require_file(MODEL, MODEL_SHA256)
require_file(DICTIONARY, DICTIONARY_SHA256)
