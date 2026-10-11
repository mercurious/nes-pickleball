#!/usr/bin/env python3
# RetroArch network-command client (UDP 55355): READ_CORE_MEMORY / WRITE_CORE_MEMORY / SHOW_MSG / SCREENSHOT ...
# Adapted from autogamer's app/ra.py (mercurious, MIT). Usage: ra.py CMD...   e.g. ra.py GET_STATUS
import socket, sys

class RA:
    def __init__(self, host="127.0.0.1", port=55355, timeout=1.0):
        self.addr = (host, port); self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); self.s.settimeout(timeout)

    def cmd(self, line, reply=True):
        self.s.sendto(line.encode(), self.addr)
        if not reply: return None
        try: return self.s.recv(65536).decode(errors="replace").strip()
        except socket.timeout: return None

    def drain(self):
        self.s.setblocking(False)
        try:
            while True: self.s.recv(65536)
        except (BlockingIOError, OSError): pass
        self.s.settimeout(1.0)

    def ram(self, addr, n, verb="READ_CORE_MEMORY"):
        """bytes at addr..addr+n, or None on error"""
        self.drain()
        r = self.cmd("%s %x %d" % (verb, addr, n))
        if not r: return None
        p = r.split()
        if len(p) < 3 or p[2] == "-1" or p[1].lower() != "%x" % addr: return None
        return bytes(int(x, 16) for x in p[2:])

    def poke(self, addr, data, verb="WRITE_CORE_MEMORY"):
        self.drain()
        return self.cmd("%s %x %s" % (verb, addr, " ".join("%02x" % b for b in data)))

if __name__ == "__main__":
    print(RA().cmd(" ".join(sys.argv[1:])))
