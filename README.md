# dds-dyn

Unofficial Python bindings for [eProsima Fast DDS](https://github.com/eProsima/Fast-DDS) 3.6.2,
packaged as a self-contained wheel. Types come from IDL
**at runtime** (Fast DDS's built-in IDL parser and dynamic types), and samples are
plain Python dicts. There is no code generation and no compiler on the user's machine.

```python
import dds_dyn as dds

with dds.Participant(domain_id=42, name="me") as p:
    p.register_idl("Sensor.idl", "sensors::Reading")      # fully qualified name
    qos = dds.QoS(reliable=True, transient_local=True, depth=32)
    w = p.create_writer("SensorReadings", "sensors::Reading", qos)
    r = p.create_reader("SensorReadings", "sensors::Reading", qos)
    w.write({"node_id": 4, "node_name": "me", "seq": 1, "value": 1.5, "text": "hi"})
    if r.wait(1.0):                    # seconds; releases the GIL
        for sample in r.take():        # list of dicts
            print(sample)
```

## Install

```bash
pip install dds_dyn-0.1.0-cp312-abi3-manylinux_2_28_x86_64.whl   # Linux, Python 3.12+
pip install dds_dyn-0.1.0-cp312-abi3-win_amd64.whl                # Windows, Python 3.12+
```

Wheels are also built for CPython 3.9–3.11 (one per version). Fast DDS, Fast-CDR,
foonathan_memory, Asio and TinyXML2 are linked statically into the one extension
module, so the wheel has no runtime dependencies. OpenSSL and SQLite are not needed:
TLS, DDS security, SQLite persistence and statistics are compiled out.

## Why not Fast-DDS-python?

eProsima's [Fast-DDS-python](https://github.com/eProsima/Fast-DDS-python) is not on PyPI.
It does not wrap the dynamic-types API, so every IDL type needs a `fastddsgen -python`
SWIG module compiled against the exact same Fast DDS build. dds-dyn has a smaller
API, but a new IDL is just a file you load.

If you do need Fast-DDS-python, [docs/building-fastdds-python.md](docs/building-fastdds-python.md)
explains how to build it from source on Linux and Windows.

## API

| | |
|---|---|
| `Participant(domain_id=0, name="", transports="env")` | `transports`: `"env"` (Fast DDS defaults, honors `FASTDDS_BUILTIN_TRANSPORTS`), `"default"` (UDPv4 + SHM), `"udpv4"`, `"udpv6"`, `"shm"`, `"large_data"`. Context manager; `close()`. |
| `p.register_idl(path, "mod::Type", include_paths=None)` | Returns the registered type name. |
| `p.register_idl_string(idl_text, "mod::Type")` | Same, from source text. |
| `QoS(reliable=True, transient_local=False, depth=1, heartbeat_ms=0, representation="")` | `depth<=0` = KEEP_ALL. `heartbeat_ms` (writer): 0 = Fast DDS default 3 s. `representation`: `"xcdr1"`, `"xcdr2"`, or `""` (readers then accept both). |
| `p.create_writer(topic, type, qos)` → `Writer` | `write(dict)`, `matched_count()` |
| `p.create_reader(topic, type, qos)` → `Reader` | `take(max_samples=0)` → list of dicts, `wait(timeout=1.0)` → bool, `matched_count()` |
| `set_log_level("error"\|"warning"\|"info")`, `FASTDDS_VERSION` | |

**Type mapping.** struct → dict (members left out keep their defaults; unknown keys raise
`KeyError`), sequence/array → list (multi-dimensional arrays flattened), `sequence<octet>` →
`bytes`, enum → int (a literal name `str` is accepted on write), `char`/`wchar` → 1-char
str, integers range-checked, string and sequence bounds enforced, typedefs resolved.
Not supported yet: unions, maps, bitsets/bitmasks, `@optional`, keyed instances
(`@key` types register and publish, but there is no per-instance API).

**Fast DDS IDL parser fixes.** `build_deps.py` applies two patches from `patches/` to
Fast DDS 3.6.2's runtime IDL parser. Both bugs are in stock 3.6.2:

- `fastdds-3.6.2-idl-comments.patch`: three or more comments in a row outside a struct
  (a typical file header) made the whole file fail to parse ("IDL parsing failed").
  Each whitespace slot in the grammar accepted only one comment.
- `fastdds-3.6.2-idl-string-bounds.patch`: a `string<N>` bound was given to the wrong
  member. All bounds went through one shared slot, read by whichever string member was
  resolved first, so in `string a; string<8> b;` the parser made `a` bounded and `b` not.

One stock bug is not patched: `sequence<string<N>>` crashes the parser (segfault). Use a
typedef instead: `typedef string<N> ShortStr; sequence<ShortStr> names;`.

**IDL and the C preprocessor.** Without `include_paths`, the IDL file is parsed directly,
so comments, `typedef`, annotations and modules work but `#include` doesn't. With
`include_paths`, Fast DDS runs `cpp` (Linux) or `cl` (Windows, from a Developer prompt).
On a machine without a compiler, put everything into one file.

## Building wheels

`build_deps.py` builds the static C++ dependencies (5–10 min, once). The extension
build then takes about 30 s.

```bash
python build_deps.py                                  # -> deps/install
CMAKE_PREFIX_PATH=$PWD/deps/install pip install .     # or: python -m build --wheel
python tests/test_loopback.py
```

On Windows (Visual Studio 2022 with C++, CMake, git), use a short work dir because
Fast DDS's build tree is deep:

```bat
python build_deps.py --prefix C:\fdd\install --work C:\fdw
set CMAKE_PREFIX_PATH=C:\fdd\install
pip install .
```

**Release wheels** are built with [cibuildwheel](https://cibuildwheel.pypa.io): Linux
wheels in the `manylinux_2_28` container, Windows wheels on a Windows host. Settings are
in `pyproject.toml`; `.github/workflows/wheels.yml` builds both on GitHub Actions and
runs `tests/test_loopback.py` against every wheel.

```bash
pipx run cibuildwheel --platform linux    # local, needs Docker -> wheelhouse/
```

## Trademarks

dds-dyn is an independent project. It is not affiliated with, endorsed by, or supported by
eProsima. "Fast DDS" and "eProsima" are names of eProsima (Proyectos y Sistemas de
Mantenimiento SL), used here only to say which library these bindings wrap.

## Licenses

dds-dyn is Apache-2.0. Each wheel statically links Fast DDS, Fast-CDR and their bundled
third-party code, as well as nanobind, so it includes all their license texts under
`dds_dyn/licenses/` in the installed package. See [licenses/README.md](licenses/README.md).
