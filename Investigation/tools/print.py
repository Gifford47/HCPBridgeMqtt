f=open("dump2.bin","rb")
def DataToHex(data):
   res = ""
   for c in data:
      res+=("%02X " % c)
   return res

while True:
   len = ord(f.read(1))
   timestamp1 = f.read(2)
   timestamp2 = f.read(2)
   data = f.read(len)
   print("%d:%d:%04X: %s" % (timestamp1[0]<<8|timestamp1[1],timestamp2[0]<<8|timestamp2[1], len,DataToHex(data)))

f.close()
