"""End-to-end check via MPRIS and a private PulseAudio monitor (Linux)."""
import array
import math
import os
from pathlib import Path
import signal
import statistics
import subprocess
import sys
import tempfile
import time
import wave

import dbus

exe = Path(sys.argv[1]).resolve()
sink = "hero_validation_" + str(os.getpid())
module = subprocess.check_output(["pactl", "load-module", "module-null-sink",
                                  "sink_name=" + sink]).strip().decode()
try:
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "tone.wav"
        with wave.open(str(path), "wb") as out:
            out.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            block = array.array("h", (int(8000*math.sin(i*math.pi/24))
                                      for i in range(48000))).tobytes()
            for _ in range(90):
                out.writeframesraw(block)
        env = dict(os.environ, XDG_CONFIG_HOME=tmp+"/config", XDG_DATA_HOME=tmp+"/data",
                   PULSE_SINK=sink, GST_AUDIO_SINK="pulsesink")
        def launch():
            return subprocess.Popen([exe, path], env=env)
        p = launch()
        try:
            bus = dbus.SessionBus()
            service = "org.mpris.MediaPlayer2.StellarMusic"
            for _ in range(100):
                if bus.name_has_owner(service):
                    break
                assert p.poll() is None
                time.sleep(.05)
            obj = bus.get_object(service, "/org/mpris/MediaPlayer2")
            base = "org.mpris.MediaPlayer2"
            iface = base + ".Player"
            props = dbus.Interface(obj, "org.freedesktop.DBus.Properties")
            player = dbus.Interface(obj, iface)
            def rms():
                # Skip device startup; capture the last contiguous real samples.
                with subprocess.Popen(["parec", "--device="+sink+".monitor",
                                       "--rate=48000", "--channels=1", "--format=s16le",
                                       "--latency-msec=20"], stdout=subprocess.PIPE) as cap:
                    try:
                        data = cap.stdout.read(96000)
                    finally:
                        cap.terminate()
                x = array.array("h"); x.frombytes(data[-24000:])
                return math.sqrt(statistics.mean(v*v for v in x))
            levels = []
            for volume in [1., .5, 0., 1.]:
                props.Set(iface, "Volume", dbus.Double(volume))
                levels.append(rms())
            assert levels[0] > 100 and .4 < levels[1]/levels[0] < .6, levels
            assert levels[2] < 2 and .9 < levels[3]/levels[0] < 1.1, levels
            print("RMS salida 100%, 50%, 0%, 100%:", levels)
            player.Pause()
            assert props.Get(iface, "PlaybackStatus") == "Paused"
            player.Seek(dbus.Int64(2000000))
            player.Play()
            stat = Path(f"/proc/{p.pid}/stat")
            def cpu():
                fields = stat.read_text().split(") ")[1].split()
                return (int(fields[11])+int(fields[12]))/os.sysconf("SC_CLK_TCK")
            start, t = cpu(), time.monotonic()
            time.sleep(5)
            pct = (cpu()-start)*100/(time.monotonic()-t)
            status = Path(f"/proc/{p.pid}/status").read_text().splitlines()
            print("CPU 5 s (% de un núcleo):", round(pct, 1),
                  next(s for s in status if s.startswith("VmRSS:")))
            player.Stop()
            assert int(props.Get(iface, "Position")) == 0
            dbus.Interface(obj,base).Quit()
            assert p.wait(timeout=5)==0
        finally:
            if p.poll() is None:
                p.terminate()
                p.wait(timeout=5)
finally:
    subprocess.run(["pactl","unload-module",module],check=True)
