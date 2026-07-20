from __future__ import annotations
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(slots=True, frozen=True)
class RegisterItem:
    name: str
    label: str
    address: int
    unit: str = ""
    scale: float = 1.0
    signed: bool = False
    writable: bool = False
    minimum: float | None = None
    maximum: float | None = None

    def decode(self, raw: int) -> float | int:
        value = raw - 0x10000 if self.signed and raw & 0x8000 else raw
        scaled = value * self.scale
        return int(scaled) if self.scale == 1.0 else round(scaled, 4)

    def encode(self, value: float) -> int:
        raw = round(value / self.scale)
        if self.signed and raw < 0:
            raw = (raw + 0x10000) & 0xFFFF
        if not 0 <= raw <= 0xFFFF:
            raise ValueError(f"{self.label}换算后的寄存器值越界")
        return raw


class RegisterMap:
    GROUPS = ("coils", "discrete_inputs", "input_registers", "holding_registers")

    def __init__(self, path: str | Path) -> None:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        self.poll_interval_ms = int(data.get("poll_interval_ms", 500))
        self.groups = {group: [RegisterItem(**x) for x in data.get(group, [])]
                       for group in self.GROUPS}

    def items(self, group: str) -> list[RegisterItem]:
        return self.groups.get(group, [])

    @staticmethod
    def span(items: list[RegisterItem]) -> tuple[int, int] | None:
        if not items:
            return None
        start = min(x.address for x in items)
        end = max(x.address for x in items)
        return start, end - start + 1

    @staticmethod
    def spans(items: list[RegisterItem]) -> list[tuple[int, int]]:
        if not items:
            return []
        ordered = sorted(items, key=lambda x: x.address)
        blocks: list[tuple[int, int]] = []
        block_start = ordered[0].address
        block_end = ordered[0].address
        for i in range(1, len(ordered)):
            addr = ordered[i].address
            if addr - block_end <= 1:
                block_end = addr
            else:
                blocks.append((block_start, block_end - block_start + 1))
                block_start = addr
                block_end = addr
        blocks.append((block_start, block_end - block_start + 1))
        return blocks

    def decode_group(self, group: str, start: int,
                      values: list[int] | list[bool],
                      block_items: list[RegisterItem] | None = None) -> dict[str, Any]:
        result: dict[str, Any] = {}
        items = block_items if block_items is not None else self.items(group)
        for item in items:
            raw = values[item.address - start]
            result[item.name] = bool(raw) if group in ("coils", "discrete_inputs") else item.decode(int(raw))
        return result
