import argparse
from app.crc import append_crc, crc16_modbus
p=argparse.ArgumentParser(description='CRC16/MODBUS计算工具'); p.add_argument('hex_data'); a=p.parse_args(); data=bytes.fromhex(a.hex_data); crc=crc16_modbus(data)
print(f'CRC = 0x{crc:04X}'); print('完整帧 =', append_crc(data).hex(' ').upper())
