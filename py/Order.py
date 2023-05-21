from __future__ import annotations
from typing import Callable, TypeVar, Optional
import struct
import difflib

UINT64_SIZE = 8
DOUBLE_SIZE = 8

T = TypeVar('T')

def ratio(a: str, b: str) -> float:
    return difflib.SequenceMatcher(a=a, b=b).ratio()

def int_to_uint64(a: int) -> bytes:
    return struct.pack('L', a) # TODO: 'L' or 'Q' ?

def uint64_to_int(b: bytes) -> int:
    assert len(b) == UINT64_SIZE

    return struct.unpack('L', b)[0]

def float_to_double(f: float) -> bytes:
    return struct.pack('d', f)

def double_to_float(b: bytes) -> float:
    assert len(b) == DOUBLE_SIZE

    return struct.unpack('d', b)[0]

def read_and_parse(
    bin: bytes,
    n: int,
    parse_fn: Callable[[bytes], T]
) -> tuple[T, bytes]:
    chunk, bin = bin[:n], bin[n:]
    
    return parse_fn(chunk), bin

class RatioList(list):
    def serialize(self) -> bytes:
        bin = bytearray()
        n_ratios = len(self)
        bin.extend(int_to_uint64(n_ratios))
        for ratio in self:
            bin.extend(float_to_double(ratio))

        return bytes(bin)
    
    @staticmethod
    def deserialize(bin: bytes) -> RatioList:
        n_ratios, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
        ratios = RatioList()
        for i in range(n_ratios):
            ratio, bin = read_and_parse(bin, DOUBLE_SIZE, double_to_float)
            ratios.append(ratio)
        
        return ratios

class Order:
    _DefaultDeserializer = None # Initialized later
    _DefaultSerializer = None # Initialized later

    def __init__(self, contents: list[str], couples: list[tuple[int, int]]):
        self.contents = contents
        self.couples = couples

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

class Deserializer:
    @staticmethod
    def deserialize(bin: bytes) -> Order:
        contents = []
        n_contents, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
        for i in range(n_contents):
            n_content_bin, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
            content, bin = read_and_parse(
                bin, n_content_bin, lambda b: b.decode()
            )
            contents.append(content)
        
        couples = []
        n_couples, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
        for i in range(n_couples):
            a, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
            b, bin = read_and_parse(bin, UINT64_SIZE, uint64_to_int)
            couples.append((a, b))
        
        assert len(bin) == 0

        return Order(contents, couples)

class Serializer:
    @staticmethod
    def serialize(order: Order) -> bytes:
        bin = bytearray()

        n_contents = len(order.contents)
        bin.extend(int_to_uint64(n_contents))
        
        for content in order.contents:
            content_bin = content.encode('utf-8')
            n_bin = len(content_bin)

            bin.extend(int_to_uint64(n_bin))
            bin.extend(content_bin)

        n_couples = len(order.couples)
        bin.extend(int_to_uint64(n_couples))

        for couple in order.couples:
            a, b = couple
            bin.extend(int_to_uint64(a))
            bin.extend(int_to_uint64(b))
        
        return bytes(bin)

Order._DefaultSerializer = Serializer.serialize
Order._DefaultDeserializer = Deserializer.deserialize