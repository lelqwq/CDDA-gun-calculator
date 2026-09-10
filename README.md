# gunlab — Cataclysm-DDA 枪械数学模型

不启动游戏，搜索枪械、装配件、横向对比。所有公式逐条复刻自 Cataclysm-DDA
源码，代码注释里标了出处（`文件:行号`）。

**数据规模**：401 把枪 / 730 种弹药 / 170 个配件，由脚本从游戏数据生成。
纯标准库，无外部依赖。

---

## 功能

| 模式 | 说明 |
|---|---|
| **1) 搜索并对比枪械** | 关键词搜枪 → 选中多把 → 输出 10 列对比表 |
| **2) 单枪详情分析** | 搜索选枪 → 装配件 → 选弹药 → 输出完整分析 |

单枪详情会输出：枪械有效数据、瞄准时间线（各技能等级瞄到三档各需多少行动点）、
散布 → 命中影响（50% 好击距离、各距离命中档位）、配件库。

### 搜索

匹配**中文名 / 英文名 / id / 变体别名**，大小写不敏感。所以 `glock`、`格洛克`、
`glock_29`、`Glock 29 pistol` 都能命中同一把枪。

### 对比表

```
枪械                  技能    弹药               枪散布  瞄具散布 操控 重量     体积      每发+误差  瞄到普通档
格洛克 20 手枪        手枪    10mm Auto 被甲弹   29      60       10   780 g    480 ml    1421       77 行动点
格洛克 21 手枪        手枪    .45 ACP 被甲弹     29      60       10   835 g    490 ml    1076       77 行动点
```

> **「每发+误差」按每把枪各自的标准弹药计算**（见「弹药」列）。DDA 的后坐完全
> 来自弹药，所以跨口径时这一列不可直接横向比较。

### 模块化枪械

M4A1 这类枪的枪身只是"下机匣"——口径、重量、体积、枪管长度都由上机匣提供。
装上一个上机匣后：

- 配件列表立刻多出 **弹壳收集器 / 刺刀座 / 导轨 / 瞄具 / 管下 / 枪口** 六组槽位
- 重量体积变成 3148 g / 3310 ml（= 880+2268 / 1760+1550）
- 弹药列表按新口径过滤

---

## 编译

### CMake

```bash
cmake -B build -S .
cmake --build build --config Release
```

不指定生成器时自动选 Visual Studio，产物 `build\gunlab.exe`。
用 g++ 就加 `-G "MinGW Makefiles"`（需要 `C:\msys64\mingw64\bin` 在 PATH 里）。
本机没装 Ninja，不要用 `-G Ninja`。

### 现成的脚本

| 脚本 | 产物 |
|---|---|
| `scripts\build_msvc.bat` | `build\gunlab_msvc.exe` |
| `scripts\build_gcc.bat`  | `build\gunlab_gcc.exe`（静态链接，可拷走） |

双击即可。

### Visual Studio 里

新建空项目 → 把 `src\` 和 `src\generated\` 下的 .cpp 都加进去 →
**C/C++ → 命令行 → 其他选项**填 `/utf-8` → F5

> `/utf-8` 不能省。源码是 UTF-8，MSVC 默认按系统代码页（简体中文 936）解读，
> 会把中文字符串字面量解坏，报出一堆 `C2447 缺少函数标题` 之类的假错误。

---

## 目录结构

```
gunlab/
├── CMakeLists.txt
├── README.md
├── .gitignore / .gitattributes
├── src/
│   ├── gunlab.cpp                 公式 + 排版 + 交互
│   ├── gun_data.h                 结构体 + 常量 + 接口
│   ├── gun_data.cpp               容器 + 填充函数
│   ├── zh_cn.h                    全部中文文本
│   └── generated/                 ★ 自动生成，勿手工编辑
│       ├── gen_guns.cpp           401 把枪
│       ├── gen_ammo.cpp           730 种弹药
│       └── gen_gunmods.cpp        170 个配件
├── scripts/
│   ├── gen_gun_data.py            ★ 数据生成器
│   ├── build_msvc.bat / build_gcc.bat
│   ├── extract_zh.py              查任意字符串的官方译名
│   └── lookup_zh.py
└── build/                         构建产物（可整个删掉）
```

---

## 更新数据

游戏更新后重新生成：

```bash
python scripts/gen_gun_data.py --game "C:\Users\34068\Project\Cataclysm\CDDA"
```

加 mod：

```bash
python scripts/gen_gun_data.py --game "<路径>" --mods Aftershock,Xedra_Evolved
```

然后 `git diff` 就能看出游戏更新带来了哪些数值变化——这本身也是个有用的能力。

### 生成器做什么

1. 读 `data/json` 下全部 JSON（按类别加 mod）
2. 解析 `copy-from` 继承链，含 `proportional` / `relative`（递归到嵌套对象）
3. 从游戏的 `.mo` 翻译文件里查官方中文名
4. 输出 `src/generated/` 下三个 C++ 文件

### 生成器的几个关键规则

**变体的正式名**：若某变体的 `id` 等于条目 id，它就是这把枪的正式名，
条目自身的 `name` 反而是通用称呼（如 `glock_20` 的 "Glock pistol"）。

**别名合并**：变体不作为独立条目，而是挂成父条目的别名。220 个变体里 189 个
数值与父条目完全相同，只有 31 个有几十克的重量差——独立成条只会让对比表刷屏。

**同名条目过滤**：`data/json/ascii_art/ammo/10mm.json` 里有个
`{"type":"ascii_art","id":"10mm_fmj"}` 的字符画条目，按字母序排在 `items/`
之前。不过滤的话会用它覆盖真条目，导致弹药数值全为 0。

**中文名**：查 `.mo`，有就用，没有就保留英文原名。实测覆盖率 97.1%，
未译的 2.9% 全是 `Glock 36` / `S&W 460XVR` 这类本就该保留原文的型号名。

> 踩过的坑：Python 的 `gettext` 把部分条目存成**元组键** `('bipod', 0)` 而不是
> 字符串，必须同时试 `C.get(s)` 和 `C.get((s, 0))`，否则一条都查不到。

---

## 已知取舍

1. **`extend` / `delete` 有意跳过**。它们只操作 `effects` / `flags` 这类数组
   （弹药里 extend 动 effects 235 次、delete 动 flags 186 次），gunlab 不使用
   这些字段。若要新增依赖它们的列，需在生成器里补上。
2. **变体的重量微差被丢弃**（31 个变体，几十克量级），对散布/后坐/瞄准速度无影响。
3. **`damage.barrels` 枪管长度插值表不存储** —— 当前对比表不含伤害列。
4. **弹药只取 `recoil` / `dispersion` / `range`**，其余字段不生成。
5. **7 把枪在游戏数据里没有 `dispersion` 字段**（按 C++ 默认值 0 处理）：
   `TDI_10` / `bio_emp_gun` / `combination_gun` / `mut_longpull` /
   `ruger_lcr_22` / `takedown_recurbow_folded` / `usp_45`。
   生成时会在终端提示。这是游戏数据本身的情况，不是解析错误。
6. **公式层面的简化**：`ranged_dex_mod()` 用 `(DEX−8)×1.0` 近似（游戏里是
   `character_modifier` 表驱动）；手部操作惩罚按"健康"处理；激光瞄具的光照
   条件按"白天"处理；命中档位按目标体积 1.0 计算。

---

## 中文文本

界面文字全在 `src/zh_cn.h`。译名取自游戏本体的
`lang/mo/zh_CN/LC_MESSAGES/cataclysm-dda.mo`，用 `scripts/extract_zh.py` 提取。

`GunMod.location`（`"underbarrel"`）、`Gun.skill`（`"rifle"`）、`GunMod.id`
是**逻辑键**，代码里有 `has_mod("underbarrel")`、`g.skill == "archery"` 这类比较，
且与游戏 JSON 字段对应，**不要翻译**——但它们不会出现在输出里。
