# Third-party licenses

The `dds_dyn._core` extension module links the following code statically, so every wheel
includes these license texts (installed as `dds_dyn/licenses/`). Versions are the ones
`build_deps.py` builds; update the files here when those versions change.

| Component | Version | License | File |
|---|---|---|---|
| eProsima Fast DDS | 3.6.2 | Apache-2.0 | `Fast-DDS.txt` |
| eProsima Fast-CDR | 2.3.6 | Apache-2.0 | `Fast-CDR.txt` |
| foonathan_memory_vendor | 1.4.1 | Apache-2.0 | `foonathan_memory_vendor.txt` |
| foonathan/memory (built by the vendor package) | | Zlib | `foonathan_memory.txt` |
| Asio (bundled with Fast DDS) | | BSL-1.0 | `Asio.txt` |
| TinyXML-2 (bundled with Fast DDS) | | Zlib | `TinyXML2.txt` |
| PEGTL (bundled with Fast DDS; IDL parser) | | BSL-1.0 | `PEGTL.txt` |
| Boost headers (bundled with Fast DDS; shared memory) | | BSL-1.0 | `Boost.txt` |
| filewatch (bundled with Fast DDS) | | MIT | `filewatch.txt` |
| JSON for Modern C++ (bundled with Fast DDS) | | MIT | `nlohmann-json.txt` |
| nanobind | build-time version | BSD-3-Clause | `nanobind.txt` |
| robin-map (bundled with nanobind) | | MIT | `robin-map.txt` |

dds-dyn's own license is Apache-2.0 (`../LICENSE`, installed as `dds-dyn.txt`).
