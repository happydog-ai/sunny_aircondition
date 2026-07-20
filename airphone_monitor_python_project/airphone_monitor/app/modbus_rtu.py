from __future__ import annotations
from app.crc import append_crc, verify_crc
from app.exceptions import DeviceException, ProtocolError
from app.serial_transport import SerialTransport

EXCEPTION_TEXT = {
    1: "非法功能", 2: "非法数据地址", 3: "非法数据值", 4: "从站设备故障",
    5: "确认", 6: "从站设备忙", 8: "存储奇偶校验错误",
    10: "网关路径不可用", 11: "网关目标设备无响应",
}


class ModbusRTU:
    def __init__(self, transport: SerialTransport, device_id: int = 1,
                 timeout: float = 0.3) -> None:
        if not 1 <= device_id <= 247:
            raise ValueError("Modbus从机地址必须为1～247")
        self.transport = transport
        self.device_id = device_id
        self.timeout = timeout

    def _request(self, function: int, data: bytes) -> bytes:
        request = append_crc(bytes((self.device_id, function)) + data)
        response = self.transport.exchange_modbus(request, self.timeout)
        if not verify_crc(response):
            raise ProtocolError("Modbus响应CRC校验失败")
        if response[0] != self.device_id:
            raise ProtocolError("Modbus响应地址不匹配")
        if response[1] == (function | 0x80):
            code = response[2]
            raise DeviceException(code, EXCEPTION_TEXT.get(code, "未知Modbus异常"))
        if response[1] != function:
            raise ProtocolError("Modbus响应功能码不匹配")
        return response

    @staticmethod
    def _address_count(address: int, count: int) -> bytes:
        if not 0 <= address <= 0xFFFF:
            raise ValueError("地址超出0～65535")
        if not 1 <= count <= 125:
            raise ValueError("读取数量超出1～125")
        return bytes(((address >> 8) & 0xFF, address & 0xFF,
                      (count >> 8) & 0xFF, count & 0xFF))

    def _read_bits(self, function: int, address: int, count: int) -> list[bool]:
        response = self._request(function, self._address_count(address, count))
        payload = response[3:3 + response[2]]
        return [bool(payload[i // 8] & (1 << (i % 8))) for i in range(count)]

    def _read_registers(self, function: int, address: int,
                        count: int) -> list[int]:
        response = self._request(function, self._address_count(address, count))
        byte_count = response[2]
        if byte_count != count * 2:
            raise ProtocolError(f"寄存器响应字节数错误：{byte_count}")
        payload = response[3:3 + byte_count]
        return [(payload[i] << 8) | payload[i + 1]
                for i in range(0, len(payload), 2)]

    def read_coils(self, address: int, count: int) -> list[bool]:
        return self._read_bits(0x01, address, count)

    def read_discrete_inputs(self, address: int, count: int) -> list[bool]:
        return self._read_bits(0x02, address, count)

    def read_holding_registers(self, address: int, count: int) -> list[int]:
        return self._read_registers(0x03, address, count)

    def read_input_registers(self, address: int, count: int) -> list[int]:
        return self._read_registers(0x04, address, count)

    def write_single_coil(self, address: int, enabled: bool) -> None:
        value = 0xFF00 if enabled else 0
        data = bytes(((address >> 8) & 0xFF, address & 0xFF,
                      (value >> 8) & 0xFF, value & 0xFF))
        if self._request(0x05, data)[2:6] != data:
            raise ProtocolError("写单线圈响应回显不一致")

    def write_single_register(self, address: int, value: int) -> None:
        if not 0 <= value <= 0xFFFF:
            raise ValueError("寄存器值超出0～65535")
        data = bytes(((address >> 8) & 0xFF, address & 0xFF,
                      (value >> 8) & 0xFF, value & 0xFF))
        if self._request(0x06, data)[2:6] != data:
            raise ProtocolError("写单寄存器响应回显不一致")

    def write_multiple_registers(self, address: int, values: list[int]) -> None:
        if not values or len(values) > 123:
            raise ValueError("多寄存器写入数量必须为1～123")
        payload = bytearray()
        for value in values:
            if not 0 <= value <= 0xFFFF:
                raise ValueError("寄存器值超出0～65535")
            payload.extend(((value >> 8) & 0xFF, value & 0xFF))
        count = len(values)
        data = bytes(((address >> 8) & 0xFF, address & 0xFF,
                      (count >> 8) & 0xFF, count & 0xFF, len(payload))) + bytes(payload)
        expected = bytes(((address >> 8) & 0xFF, address & 0xFF,
                          (count >> 8) & 0xFF, count & 0xFF))
        if self._request(0x10, data)[2:6] != expected:
            raise ProtocolError("写多个寄存器响应回显不一致")
