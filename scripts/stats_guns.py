# -*- coding: utf-8 -*-
"""看一眼生成出来的枪械数据长什么样：技能分布、连发数、卡壳标志等。

从 gen_guns.cpp 里按位置解析 add_gun 调用（字段顺序见该文件的 FIELDS_NOTE）。
只读，不改任何东西。

    python scripts/stats_guns.py
"""
import io
import os
import re
import collections

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "src", "generated", "gen_guns.cpp")

RE_ID = re.compile(r'add_gun\("([^"]+)"')
RE_SKILL = re.compile(r'add_gun\("[^"]+",\s*"(?:[^"\\]|\\.)*",\s*"(?:[^"\\]|\\.)*",\s*"([a-z_]*)"')
RE_MODES = re.compile(r'\{ "([A-Z]+)", "(?:[^"\\]|\\.)*", (\d+) \}')
RE_TAIL = re.compile(r'\}, (true|false), (true|false), "core"\);')

skill = collections.Counter()
modes = collections.Counter()
multi = reload_shoot = can_jam = 0
total = 0

for ln in io.open(PATH, encoding="utf-8"):
    m = RE_ID.search(ln)
    if not m:
        continue
    total += 1
    s = RE_SKILL.search(ln)
    skill[s.group(1) if s else "(无)"] += 1

    found = RE_MODES.findall(ln)
    for mid, qty in found:
        modes[(mid, int(qty))] += 1
    if any(int(q) > 1 for _, q in found):
        multi += 1

    t = RE_TAIL.search(ln)
    if t:
        if t.group(1) == "true":
            reload_shoot += 1
        if t.group(2) == "true":
            can_jam += 1

print("枪械 %d 把" % total)
print()
print("技能分布：")
for k, v in skill.most_common():
    print("  %-12s %3d" % (k, v))
print()
print("能连发的       : %d" % multi)
print("reload_and_shoot: %d" % reload_shoot)
print("可能卡壳的     : %d" % can_jam)
print()
print("出现过的射击模式：")
for (mid, qty), n in modes.most_common(8):
    print("  %-10s %3d 发  ×%d" % (mid, qty, n))
