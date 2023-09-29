from __future__ import annotations
from typing import Callable, TypeVar, Optional, ClassVar
from dataclasses import dataclass
import io
import struct
import difflib

UINT64_SIZE = 8
DOUBLE_SIZE = 8

T = TypeVar('T')

def ratio(a: str, b: str) -> float:
    return difflib.SequenceMatcher(a=a, b=b).ratio()

def int_to_uint64(a: int) -> bytes:
    return struct.pack('=Q', a) # TODO: 'L' or 'Q' ?

def uint64_to_int(b: bytes) -> int:
    assert len(b) == UINT64_SIZE

    return struct.unpack('=Q', b)[0]

def float_to_double(f: float) -> bytes:
    return struct.pack('=d', f)

def double_to_float(b: bytes) -> float:
    assert len(b) == DOUBLE_SIZE

    return struct.unpack('=d', b)[0]

def read_and_parse(
    buff: io.BufferedIOBase,
    n: int,
    parse_fn: Callable[[bytes], T]
) -> tuple[T, bytes]:
    chunk = buff.read(n)
    assert len(chunk) == n
    return parse_fn(chunk)

class RatioList(list):
    def serialize(self) -> bytes:
        buff = io.BytesIO()
        
        n_ratios = len(self)
        buff.write(int_to_uint64(n_ratios))
        for ratio in self:
            buff.write(float_to_double(ratio))
        
        buff.seek(0)
        return buff.read()

    @staticmethod
    def deserialize(bin: bytes) -> RatioList:
        buff = io.BytesIO(bin)
        n_ratios = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
        ratios = RatioList()
        for _ in range(n_ratios):
            ratio = read_and_parse(buff, DOUBLE_SIZE, double_to_float)
            ratios.append(ratio)
        
        return ratios

@dataclass
class Order:
    _DefaultDeserializer: ClassVar[Callable[[bytes], Order]] = None # Initialized later
    _DefaultSerializer: ClassVar[Callable[[Order], bytes]] = None # Initialized later
    contents: list[str]
    couples: list[tuple[int, int]]

    def serialize(
        self,
        serializer: Optional[Callable[[Order], bytes]] = None
    ) -> bytes:
        cls = self.__class__

        if serializer is None:
            serializer = cls._DefaultSerializer
        
        return serializer(self)

    @classmethod
    def deserialize(
        cls,
        bin: bytes,
        deserialize: Optional[Callable[[bytes], Order]] = None
    ) -> Order:
        if deserialize is None:
            deserialize = cls._DefaultDeserializer
        
        return deserialize(bin)

    def execute(self) -> RatioList:
        ratios = []
        
        for (ia, ib) in self.couples:
            a = self.contents[ia]
            b = self.contents[ib]
            ratios.append(ratio(a, b))
        
        return RatioList(ratios)

    def __eq__(self, other) -> bool:    
        return self.contents == other.contents and self.couples == other.couples

# using class as a namespace
class OrderDeserializer:
    @staticmethod
    def deserialize(bin: bytes) -> Order:
        buff = io.BytesIO(bin)

        contents = []
        n_contents = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
        for _ in range(n_contents):
            n_content_bin = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
            content = read_and_parse(
                buff, n_content_bin, lambda b: b.decode()
            )
            contents.append(content)
        
        couples = []
        n_couples = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
        for _ in range(n_couples):
            a = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
            b = read_and_parse(buff, UINT64_SIZE, uint64_to_int)
            couples.append((a, b))
        
        assert len(buff.read()) == 0

        return Order(contents, couples)

# using class as a namespace
class OrderSerializer:
    @staticmethod
    def serialize(order: Order) -> bytes:
        buff = io.BytesIO()

        n_contents = len(order.contents)
        buff.write(int_to_uint64(n_contents))
        for content in order.contents:
            content_bin = content.encode('utf-8')
            n_bin = len(content_bin)

            buff.write(int_to_uint64(n_bin))
            buff.write(content_bin)

        n_couples = len(order.couples)
        buff.write(int_to_uint64(n_couples))
        for couple in order.couples:
            a, b = couple
            buff.write(int_to_uint64(a))
            buff.write(int_to_uint64(b))
        
        buff.seek(0)
        return buff.read()

Order._DefaultSerializer = OrderSerializer.serialize
Order._DefaultDeserializer = OrderDeserializer.deserialize