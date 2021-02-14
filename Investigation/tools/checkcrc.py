def calc_crc(data):
    crc = 0xFFFF
    for pos in data:
        crc ^= pos 
        for i in range(8):
            if ((crc & 1) != 0):
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc

def checkLine(l):
   data = bytearray.fromhex(l)
   crc = calc_crc(data[:len(data)-2])
   return str(crc == int.from_bytes(data[-2:],byteorder='little', signed=False))
   #return "%04X %04X"%(crc,int.from_bytes(data[-2:],byteorder='little', signed=False))
   

with open("scan.txt") as f:
   for line in f:
       if len(line.strip()) > 0:
          print(line.strip() + "\t" + checkLine(line.strip()))