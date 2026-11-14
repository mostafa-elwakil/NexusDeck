import serial, time, sys, os

port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
log = os.path.join(os.path.dirname(os.path.abspath(__file__)), "serial-log.txt")

s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = False

with open(log, "a", buffering=1, encoding="utf-8", errors="replace") as out:
    out.write("\n=== monitor started %s ===\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
    while True:
        try:
            data = s.read(512)
            if data:
                out.write(data.decode("utf-8", "replace"))
                out.flush()
        except Exception as e:
            out.write("[serial error: %s]\n" % e)
            out.flush()
            time.sleep(1)
            try:
                s.close()
            except Exception:
                pass
            time.sleep(2)
            s = serial.Serial(port, 115200, timeout=0.2)
            s.dtr = False
            s.rts = False
