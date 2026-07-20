from __future__ import annotations
import queue
import threading
import time
from pathlib import Path
from typing import Any
from PySide6.QtCore import QThread, Signal
from app.aa55_protocol import AA55Protocol
from app.exceptions import CommunicationError, DeviceException, ProtocolError
from app.modbus_rtu import ModbusRTU
from app.register_map import RegisterMap
from app.serial_transport import SerialTransport


class SerialWorker(QThread):
    connected = Signal(dict)
    disconnected = Signal()
    status_updated = Signal(dict)
    command_finished = Signal(str, object)
    communication_error = Signal(str)
    raw_log = Signal(str, str)
    statistics_updated = Signal(dict)

    def __init__(self, register_map_path: str | Path) -> None:
        super().__init__()
        self._queue: queue.Queue[tuple[str, dict[str, Any]]] = queue.Queue()
        self._stop_event = threading.Event()
        self._transport = SerialTransport(self._on_packet)
        self._map = RegisterMap(register_map_path)
        self._protocol: AA55Protocol | ModbusRTU | None = None
        self._connected = False
        self._poll_enabled = True
        self._poll_interval = self._map.poll_interval_ms / 1000.0
        self._next_poll = 0.0
        self._stats = {"tx_frames": 0, "rx_frames": 0, "errors": 0, "polls": 0}

    def submit(self, action: str, **payload: Any) -> None:
        self._queue.put((action, payload))
        if not self.isRunning():
            self.start()

    def _aa55_command(self, name: str, func, *args: Any) -> None:
        was_polling = self._poll_enabled
        self._poll_enabled = False
        t0 = time.monotonic()
        try:
            result = func(*args)
            elapsed = (time.monotonic() - t0) * 1000
            self.raw_log.emit("INFO", f"{name}成功，耗时{elapsed:.0f}ms")
            self.command_finished.emit(name, {"success": True, "data": result, "elapsed_ms": elapsed})
        except (CommunicationError, ProtocolError, DeviceException, RuntimeError) as exc:
            elapsed = (time.monotonic() - t0) * 1000
            self.raw_log.emit("INFO", f"{name}失败：{exc}")
            self.command_finished.emit(name, {"success": False, "error": str(exc), "elapsed_ms": elapsed})
        finally:
            self._poll_enabled = was_polling
            self._next_poll = 0.0

    def stop(self) -> None:
        self._stop_event.set()
        self._queue.put(("shutdown", {}))
        self.wait(2000)

    def _on_packet(self, direction: str, payload: bytes) -> None:
        self._stats["tx_frames" if direction == "TX" else "rx_frames"] += 1
        self.raw_log.emit(direction, payload.hex(" ").upper())
        self.statistics_updated.emit(dict(self._stats))

    def run(self) -> None:
        while not self._stop_event.is_set():
            try:
                action, payload = self._queue.get(timeout=0.02)
                if action == "shutdown":
                    break
                self._handle(action, payload)
            except queue.Empty:
                pass
            except Exception as exc:
                self._stats["errors"] += 1
                self.statistics_updated.emit(dict(self._stats))
                self.communication_error.emit(str(exc))

            if self._connected and self._poll_enabled and time.monotonic() >= self._next_poll:
                try:
                    self._poll()
                except Exception as exc:
                    self._stats["errors"] += 1
                    self.statistics_updated.emit(dict(self._stats))
                    self.communication_error.emit(f"自动轮询失败：{exc}")
                self._next_poll = time.monotonic() + self._poll_interval

        self._transport.close()

    def _handle(self, action: str, p: dict[str, Any]) -> None:
        if action == "connect":
            try:
                self._transport.open(str(p["port"]), int(p["baudrate"]),
                                     str(p["parity"]), 1, float(p.get("timeout", 0.3)))
                address = int(p["address"])
                if p["protocol"] == "AA55":
                    self._protocol = AA55Protocol(self._transport, address, float(p.get("timeout", 0.3)))
                    try:
                        version = self._protocol.get_version()
                    except Exception:
                        version = None
                    info = {"port": p["port"], "protocol": "AA55", "version": version}
                else:
                    self._protocol = ModbusRTU(self._transport, address, float(p.get("timeout", 0.3)))
                    info = {"port": p["port"], "protocol": "MODBUS"}
                self._connected = True
                self._next_poll = 0.0
                self.connected.emit(info)
            except Exception as exc:
                self.communication_error.emit(f"连接失败：{exc}")
                self.command_finished.emit("_connect_failed", {})
        elif action == "disconnect":
            self._transport.close()
            self._protocol = None
            self._connected = False
            self.disconnected.emit()
        elif action == "set_polling":
            self._poll_enabled = bool(p["enabled"])
        elif action == "set_poll_interval":
            self._poll_interval = max(0.1, int(p["milliseconds"]) / 1000.0)
        elif action == "set_led":
            protocol = self._require()
            if isinstance(protocol, AA55Protocol):
                self._aa55_command("set_led", protocol.set_led, bool(p["enabled"]))
            else:
                protocol.write_single_coil(int(p.get("address", 0)), bool(p["enabled"]))
                self.command_finished.emit("set_led", bool(p["enabled"]))
                self._next_poll = 0.0
        elif action == "write_coil":
            protocol = self._require_modbus()
            protocol.write_single_coil(int(p["address"]), bool(p["enabled"]))
            self.command_finished.emit("write_coil", True)
            self._next_poll = 0.0
        elif action == "write_register":
            protocol = self._require_modbus()
            protocol.write_single_register(int(p["address"]), int(p["value"]))
            self.command_finished.emit("write_register", True)
            self._next_poll = 0.0
        elif action == "manual":
            self.command_finished.emit("manual", self._manual(p))
        elif action == "ping":
            protocol = self._require()
            if isinstance(protocol, AA55Protocol):
                self._aa55_command("ping", protocol.ping)
            else:
                result = protocol.read_holding_registers(0, 1)
                self.command_finished.emit("ping", result)
        elif action == "get_version":
            protocol = self._require()
            if isinstance(protocol, AA55Protocol):
                self._aa55_command("get_version", protocol.get_version)
            else:
                raise RuntimeError("获取版本仅适用于AA55协议")

    def _require(self) -> AA55Protocol | ModbusRTU:
        if not self._connected or self._protocol is None:
            raise RuntimeError("设备尚未连接")
        return self._protocol

    def _require_modbus(self) -> ModbusRTU:
        protocol = self._require()
        if not isinstance(protocol, ModbusRTU):
            raise RuntimeError("该操作仅适用于Modbus RTU")
        return protocol

    def _poll(self) -> None:
        protocol = self._require()
        if isinstance(protocol, AA55Protocol):
            state = protocol.get_status()
            state["protocol"] = "AA55"
        else:
            state: dict[str, Any] = {"protocol": "MODBUS"}
            readers = {
                "coils": protocol.read_coils,
                "discrete_inputs": protocol.read_discrete_inputs,
                "holding_registers": protocol.read_holding_registers,
                "input_registers": protocol.read_input_registers,
            }
            for group, reader in readers.items():
                items = self._map.items(group)
                span = self._map.span(items)
                if span:
                    start, count = span
                    state.update(self._map.decode_group(group, start, reader(start, count)))
        self._stats["polls"] += 1
        self.statistics_updated.emit(dict(self._stats))
        self.status_updated.emit(state)

    def _manual(self, p: dict[str, Any]) -> Any:
        protocol = self._require()
        operation = str(p["operation"])
        if isinstance(protocol, AA55Protocol):
            if operation == "PING": return {"ping": protocol.ping()}
            if operation == "GET_VERSION": return {"version": protocol.get_version()}
            if operation == "GET_STATUS": return protocol.get_status()
            if operation == "ECHO": return {"echo": protocol.echo(bytes.fromhex(str(p.get("data", "")))).hex(" ").upper()}
            raise ValueError("AA55暂不支持该手动命令")

        address = int(p.get("address", 0)); count = int(p.get("count", 1)); text = str(p.get("data", "0")).strip()
        if operation == "01 读线圈": return protocol.read_coils(address, count)
        if operation == "02 读离散输入": return protocol.read_discrete_inputs(address, count)
        if operation == "03 读保持寄存器": return protocol.read_holding_registers(address, count)
        if operation == "04 读输入寄存器": return protocol.read_input_registers(address, count)
        if operation == "05 写单线圈":
            value = text.lower() in ("1", "true", "on", "是"); protocol.write_single_coil(address, value); return {"written": value}
        if operation == "06 写单寄存器":
            value = int(text, 0); protocol.write_single_register(address, value); return {"written": value}
        if operation == "10 写多个寄存器":
            values = [int(x.strip(), 0) for x in text.replace("，", ",").split(",") if x.strip()]
            protocol.write_multiple_registers(address, values); return {"written": values}
        raise ValueError(f"未知操作：{operation}")
