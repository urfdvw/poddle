#!/usr/bin/env python3
"""Checks the C logic (via tests/dump_logic.c) against an independent
Python reading of the spec: spoken-time words over all 1440 hour x minute
combinations (= the spec's 720 12-hour combinations, for both AM and PM),
every second of the day for all 4 progress-bar combinations in both clock
styles, the date format, the Battery Saving update schedule (exact:
next multiple of X seconds; random: see src/c/schedule.h), and the custom
period (date/time parsing, when it is active over two weeks, and its bar
and labels every second inside a few periods)."""

import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ONES = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten",
        "Eleven", "Twelve", "Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen",
        "Eighteen", "Nineteen"]
TENS = {2: "Twenty", 3: "Thirty", 4: "Forty", 5: "Fifty"}


def words(h, m):
    h12 = h % 12 or 12
    if m == 0:
        minute = "O'Clock"
    elif m < 10:
        minute = "Oh " + ONES[m - 1]
    elif m < 20:
        minute = ONES[m - 1]
    elif m % 10 == 0:
        minute = TENS[m // 10]
    else:
        minute = TENS[m // 10] + "-" + ONES[m % 10 - 1]
    return f"{ONES[h12 - 1]}|{minute}|{'AM' if h < 12 else 'PM'}"


def clock(h, m, is24):
    h %= 24
    if not is24:
        h = h % 12 or 12
    return f"{h}:{m:02d}"


def progress(mode, fmt, is24, t):
    h, m, s = t // 3600, t // 60 % 60, t % 60
    if mode == 0:  # minute
        if fmt == 0:
            nh, nm = divmod(h * 60 + m + 1, 60)
            labels = (clock(h, m, is24), clock(nh, nm, is24))
        else:
            labels = (f"00:{s:02d}", f"-00:{(60 - s) % 60:02d}")
        return f"{s}/60|{labels[0]}|{labels[1]}"
    elapsed = m * 60 + (s if fmt == 1 else 0)
    if fmt == 0:
        labels = (clock(h, 0, is24), clock(h + 1, 0, is24))
    else:
        rem = 3600 - elapsed
        labels = (f"{elapsed // 60:02d}:{elapsed % 60:02d}", f"-{rem // 60:02d}:{rem % 60:02d}")
    return f"{elapsed}/3600|{labels[0]}|{labels[1]}"


def duration(sign, sec):
    if sec < 3600:
        return f"{sign}{sec // 60:02d}:{sec % 60:02d}"
    return f"{sign}{sec // 3600:02d}:{sec // 60 % 60:02d}:{sec % 60:02d}"


def pdate(text):
    import re
    m = re.fullmatch(r"(\d{4})-(\d{2})-(\d{2})", text)
    if not m or not 1 <= int(m[2]) <= 12 or not 1 <= int(m[3]) <= 31:
        return 0
    return int(m[1]) * 10000 + int(m[2]) * 100 + int(m[3])


def ptime(text):
    import re
    m = re.fullmatch(r"(\d{2}):(\d{2})(:.*)?", text)
    if not m or int(m[1]) > 23 or int(m[2]) > 59:
        return -1
    return int(m[1]) * 60 + int(m[2])


def pactive(repeat, date, weekdays, start, end, mday, wday, t):
    if start < 0 or end <= start or not start <= t < end:
        return 0
    if repeat == 1:
        return int(date == 20261000 + mday)
    if repeat == 2:
        return int(bool(weekdays >> wday & 1))
    return int(repeat == 3)


def pprogress(start, end, fmt, is24, t):
    span = (end - start) * 60
    elapsed = (t if fmt == 1 else t // 60 * 60) - start * 60
    if fmt == 0:
        labels = (clock(start // 60, start % 60, is24), clock(end // 60, end % 60, is24))
    else:
        labels = (duration("", elapsed), duration("-", span - elapsed))
    return f"{elapsed}/{span}|{labels[0]}|{labels[1]}"


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "dump_logic")
        subprocess.check_call(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-o", exe,
                               os.path.join(ROOT, "tests", "dump_logic.c"),
                               os.path.join(ROOT, "src", "c", "time_words.c"),
                               os.path.join(ROOT, "src", "c", "labels.c"),
                               os.path.join(ROOT, "src", "c", "schedule.c"),
                               os.path.join(ROOT, "src", "c", "period.c")])
        out = subprocess.check_output([exe], text=True).splitlines()

    failures, counts = 0, {}
    for line in out:
        kind, rest = line.split(" ", 1)
        key, got = rest.split("|", 1)
        if kind == "words":
            h, m = map(int, key.split(":"))
            want = words(h, m)
        elif kind == "progress":
            mode, fmt, is24, t = map(int, key.split())
            want = progress(mode, fmt, is24, t)
        elif kind == "date":
            mo, d = map(int, key.split())
            want = f"{mo}/{d}"
        elif kind == "clamp":
            want = str(min(60, max(1, int(key))))
        elif kind == "exact":
            interval, now, ms = map(int, key.split())
            period = interval * 1000
            delay = int(got)
            # Lands on a multiple of the interval in Unix time, within one period.
            ok = 1 <= delay <= period and (now * 1000 + ms + delay) % period == 0
            want = got if ok else f"a delay in [1, {period}] reaching a multiple of {period}"
        elif kind == "pdate":
            want = str(pdate(key))
        elif kind == "ptime":
            want = str(ptime(key))
        elif kind == "pactive":
            want = str(pactive(*map(int, key.split())))
        elif kind == "pprogress":
            want = pprogress(*map(int, key.split()))
        else:  # random
            interval, rnd = map(int, key.split())
            if interval <= 40:
                x = interval * 1000
                want = str(x // 2 + rnd % x)
            else:
                x = min(interval, 55) * 1000
                y = 60000 - x
                want = str(x - y // 2 + rnd % y)
            assert 0 < int(want) < 60000
        counts[kind] = counts.get(kind, 0) + 1
        if got != want:
            failures += 1
            if failures <= 20:
                print(f"FAIL {kind} {key}: got {got!r}, want {want!r}")
    print(", ".join(f"{v} {k}" for k, v in counts.items()), f"checked; {failures} failures")
    longest = max((l for l in out if l.startswith("words")), key=lambda l: len(l.split("|")[2]))
    print("longest minute line:", longest.split("|")[2])
    sys.exit(1 if failures or counts.get("words") != 1440 else 0)


if __name__ == "__main__":
    main()
