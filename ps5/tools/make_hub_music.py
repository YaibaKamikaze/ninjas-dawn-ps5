"""[ps5 port] Make the hub's looping theme from a song file.

    python3 ps5/tools/make_hub_music.py <song.mp3|song.wav> <out.wav> [--start SECONDS] [--loop SECONDS]

The hub's mixer plays a WAV held in memory, so a 15-minute "extended" track
(174 MB as WAV) is cut to its opening plus one cycle of the music and given a
loop point: the opening plays once, then the cycle repeats. The cycle length is
found from the song itself (the strongest repeat of the loudness envelope
between 30 s and 200 s, then aligned to the sample by cross-correlation); the
last second before the end is crossfaded into what precedes the loop start, so
the jump back is seamless even where the repeats are not bit-identical. The
loop goes in a standard smpl chunk (ps5_sound.cpp reads it).

MP3 is decoded with the system's libmpg123 (Ubuntu ships it); WAV input is
read directly. Output: 48 kHz stereo 16-bit. Needs numpy.
"""
import ctypes
import struct
import sys
import wave

import numpy as np

RATE = 48000


def decode_mp3(path):
    lib = ctypes.CDLL("libmpg123.so.0")
    lib.mpg123_new.restype = ctypes.c_void_p
    lib.mpg123_open.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.mpg123_getformat.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_int),
                                     ctypes.POINTER(ctypes.c_int)]
    lib.mpg123_read.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    lib.mpg123_format_none.argtypes = [ctypes.c_void_p]
    lib.mpg123_format.argtypes = [ctypes.c_void_p, ctypes.c_long, ctypes.c_int, ctypes.c_int]
    lib.mpg123_delete.argtypes = [ctypes.c_void_p]
    lib.mpg123_init()
    err = ctypes.c_int()
    mh = lib.mpg123_new(None, ctypes.byref(err))
    if lib.mpg123_open(mh, path.encode()) != 0:
        raise SystemExit("libmpg123 cannot open %s" % path)
    rate, ch, enc = ctypes.c_long(), ctypes.c_int(), ctypes.c_int()
    lib.mpg123_getformat(mh, ctypes.byref(rate), ctypes.byref(ch), ctypes.byref(enc))
    lib.mpg123_format_none(mh)
    lib.mpg123_format(mh, rate.value, ch.value, 0xD0)  # MPG123_ENC_SIGNED_16
    buf = ctypes.create_string_buffer(1 << 16)
    done = ctypes.c_size_t()
    pcm = bytearray()
    while True:
        r = lib.mpg123_read(mh, buf, len(buf), ctypes.byref(done))
        pcm += buf.raw[:done.value]
        if r == -12:  # MPG123_DONE
            break
        if r not in (0, -11):  # OK, NEW_FORMAT
            raise SystemExit("libmpg123 error %d" % r)
    lib.mpg123_delete(mh)
    return np.frombuffer(bytes(pcm), np.int16).reshape(-1, ch.value).astype(np.float64), rate.value


def read_wav(path):
    with wave.open(path) as w:
        if w.getsampwidth() != 2:
            raise SystemExit("WAV input must be 16-bit")
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, w.getnchannels())
        return a.astype(np.float64), w.getframerate()


def resample(a, rate):
    if rate == RATE:
        return a
    n = int(len(a) * RATE / rate)
    x = np.arange(n) * (rate / RATE)
    return np.stack([np.interp(x, np.arange(len(a)), a[:, c]) for c in range(a.shape[1])], 1)


def find_cycle(mono, start):
    """The music's repeat length in samples: the envelope's strongest repeat
    from 30 s to 200 s, then aligned to the sample."""
    hop = RATE // 20
    usable = len(mono) // hop * hop
    env = np.sqrt(np.add.reduceat(mono[:usable] ** 2, np.arange(0, usable, hop)) / hop)
    env = np.log(env + 1)
    env -= env.mean()
    best = (-2.0, 0.0)
    for lag in range(30 * 20, min(200 * 20, len(env) // 2)):
        x, y = env[200:-lag], env[200 + lag:]
        n = min(len(x), 4000)
        c = np.corrcoef(x[:n], y[:n])[0, 1]
        best = max(best, (c, lag / 20))
    if best[0] < 0.8:
        raise SystemExit("no clear repeat in the song (best %.2f at %.1f s): give --loop" % best)
    s = int(start * RATE)
    lo, hi = int((best[1] - 0.15) * RATE), int((best[1] + 0.15) * RATE)
    win = mono[s:s + 2 * RATE]
    seg = mono[s + lo:s + hi + 2 * RATE]
    f, g = np.fft.rfft(seg, 2 * len(seg)), np.fft.rfft(win, 2 * len(seg))
    return lo + int(np.argmax(np.fft.irfft(f * np.conj(g))[:hi - lo])), best[0]


def main():
    args = [a for a in sys.argv[1:]]
    start = float(args[args.index("--start") + 1]) if "--start" in args else 30.0
    loop = float(args[args.index("--loop") + 1]) if "--loop" in args else None
    src, out = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or not args[i - 1].startswith("--"))][:2]
    a, rate = decode_mp3(src) if src.lower().endswith(".mp3") else read_wav(src)
    if a.shape[1] == 1:
        a = np.repeat(a, 2, 1)
    a = resample(a[:, :2], rate)
    if loop:
        cycle, score = int(loop * RATE), 1.0
    else:
        cycle, score = find_cycle(a.mean(1), start)
    s = int(start * RATE)
    end = s + cycle
    if end > len(a):
        raise SystemExit("the song is shorter than --start plus one cycle")
    clip = a[:end].copy()
    # Crossfade the last second toward the audio just before the loop start:
    # at the very end the clip then equals what naturally leads into it.
    x = RATE
    t = np.linspace(0, 1, x)[:, None]
    clip[end - x:end] = clip[end - x:end] * (1 - t) + a[s - x:s] * t
    pcm = np.clip(np.rint(clip), -32768, 32767).astype("<i2").tobytes()
    fmt = struct.pack("<HHIIHH", 1, 2, RATE, RATE * 4, 4, 16)
    smpl = struct.pack("<9I", 0, 0, 1_000_000_000 // RATE, 60, 0, 0, 0, 1, 0)
    smpl += struct.pack("<6I", 0, 0, s, end - 1, 0, 0)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    chunks += b"data" + struct.pack("<I", len(pcm)) + pcm
    with open(out, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)
    print("%s: %.1f s (opening %.1f s, cycle %.3f s, repeat score %.2f), %d bytes"
          % (out, end / RATE, start, cycle / RATE, score, 12 + len(chunks)))


if __name__ == "__main__":
    main()
