#!/usr/bin/python

import os
import signal
import socket
import sys

if len(sys.argv) != 3:
    raise Exception('usage: ' + sys.argv[0] + ' <controller_addr> <tag>')
controller_addr = sys.argv[1]
tag = sys.argv[2]

signal.signal(signal.SIGCHLD, signal.SIG_IGN)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
s.connect(controller_addr)
sock = s.makefile()
s.close()
sock.write(str(os.getpid()) + '\n')
sock.flush()

while True:
    cmd_argv = sock.readline().split()
    if not cmd_argv:
        sys.exit(0)
    elif cmd_argv[0] == 'SPAWN':
        if len(cmd_argv) != 2:
            raise Exception('usage: SPAWN <tag>')
        pid = os.fork()
        if pid == 0:
            sock.close()
            os.execl('./procd_test_drone.py',
                     'procd_test_drone.py',
                     controller_addr,
                     cmd_argv[1])
    else:
        raise Exception('invalid command: ' + cmd_argv[0])
