class CommunicationError(RuntimeError):
    """串口或协议通信错误。"""


class ProtocolError(CommunicationError):
    """收到格式错误、CRC错误或异常响应。"""


class DeviceException(ProtocolError):
    def __init__(self, code: int, message: str) -> None:
        super().__init__(f"{message}（错误码 0x{code:02X}）")
        self.code = code
