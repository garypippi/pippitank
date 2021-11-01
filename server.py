"""
画像認識モジュール
"""

import socket

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind("c2p")
s.listen(1)

c, a = s.accept()

# CHUNK = 4096

b = c.recv(4)
# b = c.recv(3)
print(b.decode())
