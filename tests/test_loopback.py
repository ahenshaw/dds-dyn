"""Round-trip every supported IDL type kind through Fast DDS in one process.

Run directly (`python tests/test_loopback.py`) or under pytest. No network peers
needed: the writer and reader share one participant.
"""
import os
import sys

import fastdds_dyn as dds

IDL = r"""
// comments and typedefs need no preprocessor
module demo {
    enum Color { RED, GREEN, BLUE };
    typedef double Meters;
    struct Point { long x; long y; };
    @appendable
    struct Everything {
        boolean flag;
        octet raw;
        char letter;
        short i16;
        unsigned short u16;
        long i32;
        unsigned long u32;
        long long i64;
        unsigned long long u64;
        float f32;
        Meters f64;
        string text;
        string<8> short_text;
        wstring wide;
        Color color;
        Point origin;
        sequence<long> numbers;
        sequence<Point, 4> path;
        sequence<string> words;
        sequence<octet> blob;
        long grid[2][3];
    };
};
"""

SAMPLE = {
    "flag": True, "raw": 255, "letter": "Z", "i16": -32768, "u16": 65535,
    "i32": -2147483648, "u32": 4294967295, "i64": -(2**63), "u64": 2**64 - 1,
    "f32": 1.5, "f64": 2.718281828459045, "text": "hello fast dds",
    "short_text": "8 chars!", "wide": "wide é中", "color": 2,
    "origin": {"x": -1, "y": 7}, "numbers": [1, 2, 3, -4],
    "path": [{"x": 1, "y": 2}, {"x": 3, "y": 4}], "words": ["a", "bc", ""],
    "blob": b"\x00\x01\xfe\xff", "grid": [1, 2, 3, 4, 5, 6],
}


def expect(exc, fn, *args):
    try:
        fn(*args)
    except exc as e:
        print(f"  ok: {exc.__name__}: {e}")
        return
    raise AssertionError(f"expected {exc.__name__}")


def test_loopback():
    dds.set_log_level("error")
    domain = int(os.environ.get("DDS_DOMAIN", "93"))
    # shared memory works in-process everywhere, so CI needs no network
    with dds.Participant(domain, "loopback", transports="shm") as p:
        name = p.register_idl_string(IDL, "demo::Everything")
        assert name == "demo::Everything", name
        qos = dds.QoS(reliable=True, transient_local=True, depth=8)
        w = p.create_writer("Everything", name, qos)
        r = p.create_reader("Everything", name, qos)

        w.write(SAMPLE)
        w.write({"color": "GREEN", "text": "partial"})   # enum by name, others default
        got = []
        for _ in range(50):
            if r.wait(0.1):
                got += r.take()
            if len(got) == 2:
                break
        assert len(got) == 2, got
        full, partial = got

        for k, v in SAMPLE.items():
            if k == "f32":
                assert abs(full[k] - v) < 1e-6
            else:
                assert full[k] == v, (k, full[k], v)
        assert partial["color"] == 1 and partial["text"] == "partial"
        assert partial["numbers"] == [] and partial["origin"] == {"x": 0, "y": 0}
        assert partial["grid"] == [0] * 6

        expect(KeyError, w.write, {"nope": 1})
        expect(ValueError, w.write, {"short_text": "nine char"})
        expect(ValueError, w.write, {"color": "PURPLE"})
        expect(ValueError, w.write, {"grid": [1, 2]})
        expect(ValueError, w.write, {"path": [{"x": 0}] * 5})
        expect(TypeError, w.write, {"u16": -1})
        expect(KeyError, p.create_writer, "Other", "demo::Missing", qos)
        expect(ValueError, p.register_idl_string, IDL, "Everything")   # not fully qualified
    print(f"PASS fastdds_dyn {dds.__version__} on Fast DDS {dds.FASTDDS_VERSION}, "
          f"Python {sys.version.split()[0]} ({sys.platform})")


if __name__ == "__main__":
    test_loopback()
