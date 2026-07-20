import unittest
from app.crc import append_crc, crc16_modbus, verify_crc


class ProtocolTests(unittest.TestCase):
    def test_modbus_crc(self):
        self.assertEqual(
            append_crc(bytes.fromhex('01 03 00 00 00 01')).hex(' ').upper(),
            '01 03 00 00 00 01 84 0A',
        )

    def test_aa55_ping_crc(self):
        body = bytes.fromhex('09 01 01 01 00')
        self.assertEqual(
            (bytes.fromhex('AA 55') + append_crc(body)).hex(' ').upper(),
            'AA 55 09 01 01 01 00 A9 AD',
        )

    def test_verify(self):
        frame = bytes.fromhex('09 01 01 01 00 A9 AD')
        self.assertTrue(verify_crc(frame))
        self.assertEqual(crc16_modbus(frame[:-2]), 0xADA9)


if __name__ == '__main__':
    unittest.main()
