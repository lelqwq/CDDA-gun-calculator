# gunlab — 项目约定

Cataclysm-DDA 枪械数值计算器。不启动游戏就能查枪械数值、对比配装。
C++17 控制台程序，数据由 Python 脚本从游戏 JSON 生成。

---

## 一、最重要的前提：版本对齐

**游戏本体与源码仓库不是同一个版本。** 这是本项目最容易踩的坑，改公式前必读。

| | 版本 | 位置 |
|---|---|---|
| **游戏**（数据来源） | **0.I 稳定版**，`27939e2`，2026-06-06 | `../CDDA/` |
| **源码**（公式来源） | master，0.J 开发中，2026-09+ | `../Cataclysm-DDA/` |

**数据从游戏目录读，公式从源码仓库读。** 两者差了三个月。

已逐项对比过 0.I 与 master 的公式，**没有变化**：`gun_recoil`、
`dispersion_considering_length`、`effective_dispersion`、`aim_per_move`、
`multi_lerp`、`dispersion_sources::roll` 全部逐行一致。

**但显示格式变了**：

```cpp
// 0.I  src/item.cpp:3287  —— 显示原始内部值，且分项相加
info.emplace_back("GUN", _("Dispersion: "), "", ..., mod->gun_dispersion(false, false));
// → "散布: 179+65 = 244"

// 0.J  src/item_info.cpp:1252 —— 改成单值并除以 100
double gun_dispersion = mod->gun_dispersion(false, false) / 100.0;
// → "Dispersion: 1.79 MOA"
```

**gunlab 对齐的是 0.I**（用户实际在玩的版本）。改任何显示相关的逻辑前，
先确认 0.I 怎么写：

```bash
cd ../Cataclysm-DDA
git show 27939e29b8b4ddc081490d9f51de59a459c88df6:src/item.cpp | grep -n -A10 "Dispersion: "
```

> 注意：0.I 里**没有 `src/item_info.cpp`**，枪械显示还在 `item.cpp` 里
> （那个文件是 0.J 才拆出来的）。0.I 也没有 `item_gun_tool_ammo.cpp`，
> `gun_recoil` 等函数都在 `item.cpp`。

---

## 二、构建

三种方式产物名不同，**交叉使用不会冲突**：

| 方式 | 命令 | 产物 |
|---|---|---|
| CMake | `cmake -B build -S . && cmake --build build --config Release` | `build/gunlab.exe` |
| MSVC 脚本 | 双击 `scripts/build_msvc.bat` | `build/gunlab_msvc.exe` |
| GCC 脚本 | 双击 `scripts/build_gcc.bat` | `build/gunlab_gcc.exe` |

### 三个必踩的坑

1. **MSVC 必须加 `/utf-8`**（已写在 CMakeLists 和 .bat 里）。
   不加会按系统代码页 936 解读源码，中文字面量被解坏，报出一堆
   `C2447 缺少函数标题` 之类的**假语法错误**。

2. **MinGW 必须 `-static`，且 `C:\msys64\mingw64\bin` 要在 PATH 里**。
   否则编译静默失败（退出码 1、无任何错误信息），或生成的 exe 双击时
   报缺 `libgcc_s_seh-1.dll` / `libstdc++-6.dll`。

3. **编译前要关掉正在运行的 gunlab.exe**，否则链接器报
   `LNK1104: 无法打开文件`。用 `Get-Process gunlab* | Stop-Process` 收拾。

### 环境

- Python：`C:\Python314\python.exe`
- g++：`C:\msys64\mingw64\bin\g++.exe`（GCC 16.2）
- MSVC：Visual Studio 18 Community，`14.50.35717`
- **没装 Ninja**，不要用 `-G Ninja`

---

## 三、数据生成

数据**全是生成的，不要手改** `src/generated/` 下的文件。

```bash
python scripts/gen_gun_data.py --game "C:\Users\34068\Project\Cataclysm\CDDA"
python scripts/gen_gun_data.py --game "<路径>" --mods Aftershock,Xedra_Evolved
```

产出 401 把枪 / 730 种弹药 / 170 个配件。脚本会硬排除 `TEST_DATA`
和 `Generic_Guns`（后者是互斥的全面转换 mod）。

### 生成器里几个非显然的规则

改脚本前先理解这几条，它们都是踩坑后加上的：

- **只索引 `type == "ITEM"` 或带 subtypes 的条目。**
  `data/json/ascii_art/ammo/10mm.json` 里有个 `{"type":"ascii_art","id":"10mm_fmj"}`，
  按字母序排在 `items/` 之前。不过滤的话它会覆盖真条目，**导致 730 种弹药数值全为 0**。

- **变体命名优先级**：变体 id == 条目 id → 只有一个变体 → 条目自身 name。
  漏掉第二条时 `modular_m16_auto_rifle`（只有一个变体 M16A3）会显示成
  "M16 全自动步枪"而不是游戏里的 "M16A3 步枪"。

- **`extend` / `delete` 有意跳过**。它们只操作 `effects` / `flags` 数组
  （弹药里 extend 动 effects 235 次、delete 动 flags 186 次），gunlab 不用这些字段。

- **`proportional` / `relative` 必须递归处理嵌套对象**，否则 `damage.amount`
  这类嵌套字段不会更新（如 `10mmP` 的 `relative: { damage: { amount: 2 } }`）。

- **缺省值要对齐 C++ 而不是 0**：`sight_dispersion` 默认 **30**
  （`item_factory.cpp:3411`）；`handling` 缺省传 -1 表示"按类型自动取"，
  在 `add_gun()` 里解析成步枪/SMG/霰弹枪 20、其余 10。

### 中文译名

来自游戏本体的编译翻译文件：

```
<游戏目录>\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo
```

查不到就用英文原名（覆盖率 97.1%，未译的 2.9% 全是 `Glock 36`、
`S&W 460XVR` 这类本就该保留原文的型号名）。

> **坑**：Python 的 `gettext` 把部分条目存成**元组键** `('bipod', 0)` 而不是
> 字符串，必须同时试 `C.get(s)` 和 `C.get((s, 0))`，否则一条都查不到。

---

## 四、怎么验证数值对不对

**gunlab 的「游戏内显示值」区块就是游戏物品面板上显示的数字**，可以逐项核对。
游戏里看一把枪的物品详情，gunlab 里搜同一把枪，数字应该完全一致。

已实测确认一致的（M16A3 + .223长管上机匣 + 可调节枪托 + 5.56 NATO M855，
技能5/敏捷12/感知12/力量11）：

```
散布        179+65 = 244      ✅
瞄准散布    40+9 = 49         ✅（总值一致）
实际后坐    105               ✅
理论最小    105（所需力量 10）  ✅
```

改公式后请用这个用例回归。

### 几个容易误判成 bug 的现象

- **「总散布」比「瞄准误差」多一个常数** —— 那是"固定散布"
  （枪身+弹药散布 ÷18、敏捷修正、技能不足惩罚），**不随瞄准进度变化**。
  表里已单列出来。
- **概率表里同一个百分比在多行重复出现** —— 数学必然：偏移量与距离成正比、
  判定阈值也随距离等比放大，两者抵消，不同距离的不同档位会卡在同一个
  散布投骰阈值上。
- **实际后坐 > 理论最小后坐力** —— 不是 bug，是力量不足
  （`力量 × 333g < gun_base_weight` 时后坐被放大）。

---

## 五、已知取舍

1. **弹匣不建模** —— 游戏物品装了弹匣会增重（STANAG 30 约 150g），gunlab 不算。
2. **体积合成与游戏有出入** —— `integral_volume` 的处理没完全对标
   （实测 M16A3 差约 220ml），影响「体积因子」进而轻微影响瞄准速度。
3. **`damage.barrels` 枪管长度伤害插值表未存储** —— 当前输出不含伤害列。
4. **公式层面的简化**：`ranged_dex_mod()` 用 `(DEX−8)×1.0` 近似
   （游戏里是 `character_modifier` 表驱动）；手部操作惩罚按"健康"处理；
   激光瞄具光照按"白天"；目标体积固定 1.0 格。
5. **7 把枪在游戏数据里没有 `dispersion` 字段**（`usp_45` 是其中之一），
   按 C++ 默认值 0 处理，生成时会在终端提示。

---

## 六、工作约定

### 提交

Conventional Commits 前缀（`feat:` / `fix:` / `refactor:` / `docs:` / `data:`），
**正文用中文**。正文要写清「为什么改」和「改了会怎样」——尤其是那些
从代码看不出来的取舍和踩过的坑。

**分阶段提交**，每完成一个可验证的阶段提交一次。

### 改动的验证要求

改完必须**实际编译并运行**，不能只保证"编译通过"。三种构建方式都过一遍。
涉及数值的改动，用第四节的用例对照游戏。

### 用户偏好

- 输出**对齐游戏界面**，不要暴露内部刻度（`÷18`、原始 JSON 值这类
  已从详情页移除，因为和游戏对不上容易被误当成错误）。
- **诚实标注**：哪些验证过、哪些没有、哪些是简化——直接写出来，
  不要含糊过去。
- 界面文字全在 `src/zh_cn.h`，改文案只动那个文件。
- 数值数据全在 `src/generated/`（生成）与 `src/gun_data.cpp`（容器），
  改数据不要碰 `src/gunlab.cpp`。

### 保留英文的三类标识

`GunMod.location`（`"underbarrel"`）、`Gun.skill`（`"rifle"`）、`GunMod.id`
是**逻辑键**，代码里有 `has_mod("underbarrel")`、`g.skill == "archery"`
这类比较，且与游戏 JSON 字段一一对应。**不要翻译**——但它们不会出现在
输出里，显示时一律经过 `zh::slot()` / `zh::skill()`。
