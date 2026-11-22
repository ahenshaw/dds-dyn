"""Fast DDS 3.6 for Python, with types loaded from IDL at runtime.

    import fastdds_dyn as dds

    with dds.Participant(domain_id=0, name="me") as p:
        p.register_idl("Sensor.idl", "sensors::Reading")
        qos = dds.QoS(reliable=True, transient_local=True, depth=32)
        w = p.create_writer("SensorReadings", "sensors::Reading", qos)
        r = p.create_reader("SensorReadings", "sensors::Reading", qos)
        w.write({"node_id": 4, "seq": 1, "value": 1.5, "text": "hi"})
        if r.wait(1.0):
            for sample in r.take():          # list of dicts
                print(sample)

Samples are dicts keyed by member name; nested structs are dicts, sequences and
arrays are lists (sequence<octet> is bytes), enums are ints (str literal names are
accepted on write). Members left out of a dict keep their default value.
"""
from __future__ import annotations

import os
import tempfile
from typing import Iterable, Optional

from . import _core
from ._core import Reader, Writer, set_log_level

__all__ = ["Participant", "QoS", "Reader", "Writer", "set_log_level", "FASTDDS_VERSION", "__version__"]
__version__ = "0.1.0"
FASTDDS_VERSION: str = _core.FASTDDS_VERSION


def QoS(reliable: bool = True, transient_local: bool = False, depth: int = 1,
        heartbeat_ms: int = 0, representation: str = "") -> _core.QoS:
    """Endpoint QoS.

    reliable        RELIABLE (True) or BEST_EFFORT
    transient_local TRANSIENT_LOCAL (True) or VOLATILE durability
    depth           KEEP_LAST depth; 0 or less = KEEP_ALL
    heartbeat_ms    writer HEARTBEAT period; 0 = Fast DDS default (3 s)
    representation  "xcdr1" / "xcdr2" / "" (default: writer = Fast DDS default,
                    reader = accept both XCDR2 and XCDR1)
    """
    q = _core.QoS()
    q.reliable, q.transient_local, q.depth = reliable, transient_local, depth
    q.heartbeat_ms, q.representation = heartbeat_ms, representation
    return q


class Participant(_core.Participant):
    """A DomainParticipant with one Publisher and one Subscriber.

    transports: "env" (Fast DDS defaults, honors FASTDDS_BUILTIN_TRANSPORTS),
    "default" (UDPv4 + shared memory), "udpv4", "udpv6", "shm", "large_data".
    """

    def __init__(self, domain_id: int = 0, name: str = "", transports: str = "env") -> None:
        super().__init__(domain_id, name, transports)

    def register_idl(self, idl_path: "str | os.PathLike[str]", type_name: str,
                     include_paths: Optional[Iterable[str]] = None) -> str:
        """Load `type_name` (fully qualified, e.g. "module::Struct") from an IDL file
        and register it. Returns the registered type name.

        Without include_paths the file is parsed directly (no C preprocessor, so no
        #include). With include_paths Fast DDS runs `cpp` (Linux) / `cl` (Windows).
        """
        return super().register_idl(os.fspath(idl_path), type_name, list(include_paths or []))

    def register_idl_string(self, idl: str, type_name: str) -> str:
        """Like register_idl, from IDL source text."""
        fd, path = tempfile.mkstemp(suffix=".idl")
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                f.write(idl)
            return self.register_idl(path, type_name)
        finally:
            os.unlink(path)

    def create_writer(self, topic: str, type_name: str, qos: Optional[_core.QoS] = None) -> Writer:
        return super().create_writer(topic, type_name, qos if qos is not None else QoS())

    def create_reader(self, topic: str, type_name: str, qos: Optional[_core.QoS] = None) -> Reader:
        return super().create_reader(topic, type_name, qos if qos is not None else QoS())

    def __enter__(self) -> "Participant":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
