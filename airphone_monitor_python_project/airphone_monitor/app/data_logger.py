from __future__ import annotations
import csv
from datetime import datetime
from pathlib import Path
from typing import Any


class CsvDataLogger:
    def __init__(self) -> None:
        self._file = None
        self._writer = None

    @property
    def active(self) -> bool:
        return self._file is not None

    def start(self, path: str | Path) -> None:
        self.stop()
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        self._file = target.open("w", encoding="utf-8-sig", newline="")
        self._writer = None

    def write(self, data: dict[str, Any]) -> None:
        if self._file is None:
            return
        row = {"timestamp": datetime.now().isoformat(timespec="milliseconds"), **data}
        if self._writer is None:
            self._writer = csv.DictWriter(self._file, fieldnames=list(row.keys()))
            self._writer.writeheader()
        self._writer.writerow(row)
        self._file.flush()

    def stop(self) -> None:
        if self._file is not None:
            self._file.close()
        self._file = None
        self._writer = None
