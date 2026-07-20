from __future__ import annotations
from dataclasses import dataclass
from app.crc import append_crc, verify_crc
from app.exceptions import DeviceException, ProtocolError
from app.serial_transport import SerialTransport

CTRL_REQUEST = 0x00
CTRL_RESPONSE = 0x80
CTRL_ERROR = 0xC0
CMD_PING = 0x01
CMD_GET_VERSION = 0x02
CMD_ECHO = 0x10
CMD_LED_SET = 0x20
CMD_GET_STATUS = 0x21
CMD_STORE_CONFIG = 0x30
CMD_LOAD_CONFIG = 0x31
CMD_RESTORE_DEFAULTS = 0x32
CMD_GET_CONFIG_STATUS = 0x33
CMD_GET_CONFIG_DATA = 0x34
ERROR_TEXT = {1: "未知命令", 2: "数据长度错误", 3: "数据内容错误", 4: "设备忙", 5: "设备内部错误"}
STATE_TEXT = {0: "空闲", 1: "待保存", 2: "保存中", 3: "错误"}


@dataclass(slots=True)
class AA55Response:
    address: int
    sequence: int
    command: int
    control: int
    data: bytes
    raw: bytes


class AA55Protocol:
    def __init__(self, transport: SerialTransport, address: int = 1,
                 timeout: float = 0.3) -> None:
        self.transport = transport
        self.address = address
        self.timeout = timeout
        self._sequence = 0

    def _next_sequence(self) -> int:
        self._sequence = (self._sequence + 1) & 0xFF
        return self._sequence

    def build_request(self, command: int, data: bytes = b"", *,
                      sequence: int | None = None) -> bytes:
        if len(data) > 32:
            raise ValueError("AA55协议数据区最大32字节")
        seq = self._next_sequence() if sequence is None else sequence & 0xFF
        body = bytes((9 + len(data), self.address, seq,
                      command & 0xFF, CTRL_REQUEST)) + data
        return b"\xAA\x55" + append_crc(body)

    def transact(self, command: int, data: bytes = b"") -> AA55Response:
        sequence = self._next_sequence()
        request = self.build_request(command, data, sequence=sequence)
        raw = self.transport.exchange_aa55(request, self.timeout)
        if not verify_crc(raw[2:]):
            raise ProtocolError("AA55响应CRC校验失败")
        if raw[2] != len(raw):
            raise ProtocolError("AA55响应LEN与实际长度不一致")
        address, seq, cmd, control = raw[3:7]
        payload = raw[7:-2]
        if address != self.address:
            raise ProtocolError("AA55响应地址不匹配")
        if seq != sequence:
            raise ProtocolError("AA55响应序号不匹配")
        if cmd != command:
            raise ProtocolError("AA55响应命令不匹配")
        if control == CTRL_ERROR:
            code = payload[0] if payload else 0xFF
            raise DeviceException(code, ERROR_TEXT.get(code, "未知设备错误"))
        if control != CTRL_RESPONSE:
            raise ProtocolError(f"AA55响应控制字非法：0x{control:02X}")
        return AA55Response(address, seq, cmd, control, payload, raw)

    def ping(self) -> bool:
        return self.transact(CMD_PING).data == b""

    def get_version(self) -> str:
        data = self.transact(CMD_GET_VERSION).data
        if len(data) == 3 and data[1] == ord('.'):
            return data.decode("ascii", errors="replace")
        if len(data) >= 3:
            return ".".join(str(v) for v in data[:3])
        return data.hex(" ").upper()

    def echo(self, data: bytes) -> bytes:
        return self.transact(CMD_ECHO, data).data

    def set_led(self, enabled: bool) -> bool:
        data = self.transact(CMD_LED_SET, bytes((0 if enabled else 1,))).data
        return data[0] == 0 if data else enabled

    def get_status(self) -> dict[str, int | bool]:
        data = self.transact(CMD_GET_STATUS).data
        result: dict[str, int | bool] = {"led": data[0] == 0 if data else False}
        if len(data) >= 2:
            result["rx_overflow"] = bool(data[1])
        if len(data) >= 3:
            result["uart_error_count"] = data[2]
        return result

    def store_config(self) -> int:
        data = self.transact(CMD_STORE_CONFIG).data
        return data[0] if data else 255

    def load_config(self) -> int:
        data = self.transact(CMD_LOAD_CONFIG).data
        return data[0] if data else 255

    def restore_defaults(self) -> int:
        data = self.transact(CMD_RESTORE_DEFAULTS).data
        return data[0] if data else 255

    def get_config_status(self) -> dict:
        data = self.transact(CMD_GET_CONFIG_STATUS).data
        if not data or len(data) < 12:
            return {"error": "no data"}
        return {
            "state": data[0],
            "eeprom_online": data[1],
            "active_slot": data[2],
            "dirty": data[3],
            "last_result": data[4],
            "sequence": data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24),
            "save_count": data[9] | (data[10] << 8),
            "config_source": data[11],
        }

    def get_config_data(self) -> list[int]:
        data = self.transact(CMD_GET_CONFIG_DATA).data
        return [data[i] | (data[i + 1] << 8) for i in range(0, len(data), 2)]
