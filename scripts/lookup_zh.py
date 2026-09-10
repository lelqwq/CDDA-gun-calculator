# -*- coding: utf-8 -*-
"""从 CDDA 游戏的编译翻译文件 (.mo) 中查询字符串的中文译文。"""
import gettext
import sys

MO = r"C:\Users\34068\Project\Cataclysm\CDDA\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo"

with open(MO, "rb") as f:
    T = gettext.GNUTranslations(f)

# 配件名（来自 data/json/items/gunmod/*.json 的 name.str）
NAMES = [
    "bipod", "forward grip", "offset grip", "high end hand guard",
    "recoil stock", "adjustable stock", "folding stock", "cheek pad",
    "muzzle brake", "compensator", "suppressor", "barrel_ported",
    "ported barrel", "shortened barrel",
    "red dot sight", "holographic sight", "rifle scope", "sight magnifier",
    "underbarrel laser sight", "rail laser sight",
    "army flashlight-laser module",
    "grip", "muzzle", "barrel", "stock", "sights", "rail", "underbarrel",
    # 槽位名
    "stock accessory", "mechanism", "bore", "sling", "muzzle",
    # 常见物品（对照组）
    "flashlight", "knife", "backpack", "hammer",
]

print("=== 游戏内官方中文（来自 cataclysm-dda.mo）===")
print()
missing = []
for n in NAMES:
    r = T.gettext(n)
    if r == n:                      # gettext 找不到时原样返回
        missing.append(n)
        print("  %-32s => (未翻译)" % n)
    else:
        print("  %-32s => %s" % (n, r))

print()
print("未翻译/未收录: %d / %d" % (len(missing), len(NAMES)))
