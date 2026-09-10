# gunlab — Cataclysm-DDA 枪械数学模型

不启动游戏，手动输入枪械 / 弹药 / 配件 / 人物数据，直接算出：

- 后坐力、散布、瞄准速度
- 瞄准到各档位（普通 / 仔细 / 精准）需要多少行动点
- 一回合（100 行动点）能把瞄准误差降低多少
- 每开一枪增加多少瞄准误差
- 散布 → 50% 好击距离，以及各距离下的命中档位

所有公式逐条复刻自 Cataclysm-DDA 源码，代码注释里标了出处（`文件:行号`）。
纯标准库，无外部依赖。

---

## 编译

### 方式一：CMake（推荐）

```bash
cmake -B build -S .
cmake --build build --config Release
```

不指定生成器时，CMake 会自动选 **Visual Studio 18 2026**，产物是 `build\gunlab.exe`。

想用 g++ 编译就显式指定（需要 `C:\msys64\mingw64\bin` 在 PATH 里）：

```bash
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
```

> 本机没装 Ninja，所以不要用 `-G Ninja`。

### 方式二：现成的脚本

| 脚本 | 用途 |
|---|---|
| `scripts\build_msvc.bat` | 用 Visual Studio 的 MSVC 编译 |
| `scripts\build_gcc.bat`  | 用 MSYS2 的 MinGW-w64 g++ 编译 |

双击即可。两者都会把产物输出到 `build\`。

### 方式三：在 Visual Studio 里

新建「空项目」→ 把 `src\` 下的 4 个文件加进去 →
**项目属性 → C/C++ → 命令行 → 其他选项**填 `/utf-8` → F5

> `/utf-8` 不能省。源码是 UTF-8，而 MSVC 默认按系统代码页（简体中文是 936）解读，
> 会把中文字符串字面量解坏，报出一堆莫名其妙的 `C2447 缺少函数标题` 之类的假错误。

---

## 运行

```
build\gunlab_msvc.exe     ← MSVC 版，366 KB
build\gunlab_gcc.exe      ← GCC 静态版，2.9 MB（自包含，可拷到别的机器）
```

依次选枪 → 选弹药 → 装配件（同槽位自动替换）→ 输人物属性（**直接回车用默认值**）。

---

## 目录结构

```
gunlab/
├── CMakeLists.txt          构建配置
├── README.md               本文件
├── .gitignore
├── src/                    所有源码
│   ├── gunlab.cpp              数学公式 + 输出排版 + main
│   ├── gun_data.h              结构体 + 常量 + 接口声明
│   ├── gun_data.cpp        ★   内置数据库（改数值只动这里）
│   └── zh_cn.h                 全部中文文本
├── scripts/                辅助脚本
│   ├── build_msvc.bat
│   ├── build_gcc.bat
│   ├── extract_zh.py           从游戏 .mo 文件提取官方中文译名
│   └── lookup_zh.py            简易版查询工具
└── build/                  构建产物（自动生成，可整个删掉）
```

---

## 怎么改数据

**只动 `src/gun_data.cpp`**，`gunlab.cpp` 一个字都不用改。

### 加一把枪

在 `init_database()` 的「枪械」段照抄一段：

```cpp
{   // AK-47
    Gun gun;
    gun.id   = "ak47";
    gun.name = "AK-47 突击步枪";
    gun.skill            = "rifle";
    gun.dispersion       = 180;      // data/json/items/gun/762.json
    gun.sight_dispersion = 40;       // gun_base_rifle_semi
    gun.handling         = 20;
    gun.weight_g         = 4300;
    gun.volume_ml        = 2500;
    gun.min_cycle_recoil = 1830;
    g_guns.push_back(gun);
}
```

### 加一个配件

在「配件」段加一行：

```cpp
g_mods.push_back(mk_mod("id", "中文名", "槽位", 操控修正, 瞄准修正, 瞄准散布, 视野));
```

`mk_mod` 的第 4~7 个参数可省略，默认全部为 0 / -1。

### 加一种弹药

```cpp
{ Ammo a; a.id="x"; a.name="中文名"; a.recoil=750; a.dispersion=50; a.range=14; g_ammo.push_back(a); }
```

---

## 必须保留英文的三个字段

`GunMod.location`（`"underbarrel"`）、`Gun.skill`（`"rifle"`）、`GunMod.id`（`"bipod"`）
是**逻辑键**，代码里有 `has_mod("underbarrel")`、`g.skill == "archery"` 这类比较，
而且与游戏 JSON 的字段一一对应。**不要翻译成中文**——但它们不会出现在输出里，
显示时一律经过 `zh::slot()` / `zh::skill()` 转换。

---

## 中文译名的来源

`src/zh_cn.h` 里的译名取自游戏本体的编译翻译文件：

```
<游戏目录>\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo
```

用 `scripts/extract_zh.py` 提取。想查别的字符串，改脚本里的查询列表即可。

> 踩过的坑：Python 的 `gettext` 会把部分条目存成**元组键** `('bipod', 0)` 而不是
> 字符串，所以必须同时尝试 `C.get(s)` 和 `C.get((s, 0))`，否则会查不到。

---

## 已知简化

以下几处与游戏源码有出入，代码注释里也标了：

1. `ranged_dex_mod()` 用 `(DEX − 8) × 1.0` 近似，游戏里是 `character_modifier` 表驱动
2. 弹药散布没做枪管长度插值（`dispersion_considering_length`）
3. 手部操作惩罚（`ranged_dispersion_manip_mod`）按"健康"处理
4. 激光瞄具的光照条件按"白天"处理
5. 命中档位表按目标体积 1.0 计算
6. `Ammo` 的 `damage` / `armor_pen` / `loudness` 字段存在但**未参与计算**
