#!/usr/bin/env python3
import json
import math
import sys

j = sys.stdin.read()

i = json.loads(j)
s = i["sample"]
points = [
    (s["x0"], s["y0"]),
    (s["x1"], s["y1"]),
    (s["x2"], s["y2"])]

def angle(c, l, r):
    result = math.atan2(r[1] - c[1], r[0] - c[0]) - \
             math.atan2(l[1] - c[1], l[0] - c[0])
    if result > math.pi:
        result -= math.pi * 2
    if result < -math.pi:
        result += math.pi * 2
    return result

angles = [
        angle(points[0], points[1], points[2]),
        angle(points[1], points[0], points[2]),
        angle(points[2], points[0], points[1])
    ]

acute = all(abs(a) < math.pi / 2 for a in angles)

ret = {
    "replicate": None,
    "preds": {
        "acute": acute
    },
    "aux": {
        "angles": angles
    }
}

print(json.dumps(ret))
