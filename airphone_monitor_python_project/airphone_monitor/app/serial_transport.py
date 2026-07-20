from __future__ import annotations

import time
from collections.abc import Callable
import serial
from app.exceptions import CommunicationError, ProtocolError

LogCallback = Callable[[str, bytes], None]


class SerialTransport:
    """串口传输层。仅在通信线程中调用。"""

    def __init__(self, log_callback: LogCallback | None = None) -> None:
        self._serial: serial.Serial | None = None
        self._log_callback = log_callback

    @property
    def is_open(self) -> bool:
        return self._serial is not None and self._serial.is_open

    def open(self, port: str, baudrate: int, parity: str,
             stopbits: int, timeout: float) -> None:
        self.close()
        self._serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=parity,
            stopbits=stopbits,
            timeout=timeout,
            write_timeout=timeout,
        )
        self._serial.reset_input_buffer()
        self._serial.reset_output_buffer()

    def close(self) -> None:
        if self._serial is not None:
            try:
                if self._serial.is_open:
                    self._serial.close()
            finally:
                self._serial = None

    def _require_open(self) -> serial.Serial:
        if not self.is_open or self._serial is None:
            raise CommunicationError("串口未连接")
        return self._serial

    def _log(self, direction: str, payload: bytes) -> None:
        if self._log_callback is not None:
            self._log_callback(direction, payload)

    def _read_exact(self, size: int, deadline: float) -> bytes:
        ser = self._require_open()
        output = bytearray()
        while len(output) < size:
            if time.monotonic() >= deadline:
                raise CommunicationError(
                    f"响应超时：期望{size}字节，实际收到{len(output)}字节"
                )
            chunk = ser.read(size - len(output))
            if chunk:
                output.extend(chunk)
            else:
                time.sleep(0.001)
        return bytes(output)

    def exchange_aa55(self, request: bytes, timeout: float) -> bytes:
        ser = self._require_open()
        ser.reset_input_buffer()
        self._log("TX", request)
        ser.write(request)
        ser.flush()

        deadline = time.monotonic() + timeout
        prefix = self._read_exact(3, deadline)
        if prefix[:2] != b"\xAA\x55":
            raise ProtocolError("AA55响应帧头错误：" + prefix.hex(" ").upper())
        frame_length = prefix[2]
        if not 9 <= frame_length <= 41:
            raise ProtocolError(f"AA55响应长度非法：{frame_length}")
        response = prefix + self._read_exact(frame_length - 3, deadline)
        self._log("RX", response)
        return response

    def exchange_modbus(self, request: bytes, timeout: float) -> bytes:
        ser = self._require_open()
        ser.reset_input_buffer()
        self._log("TX", request)
        ser.write(request)
        ser.flush()

        deadline = time.monotonic() + timeout
        head = self._read_exact(2, deadline)
        function = head[1]
        if function & 0x80:
            response = head + self._read_exact(3, deadline)
        elif function in (0x01, 0x02, 0x03, 0x04):
            count_raw = self._read_exact(1, deadline)
            response = head + count_raw + self._read_exact(count_raw[0] + 2, deadline)
        elif function in (0x05, 0x06, 0x0F, 0x10):
            response = head + self._read_exact(6, deadline)
        else:
            raise ProtocolError(f"暂不支持解析功能码0x{function:02X}")
        self._log("RX", response)
        return response
