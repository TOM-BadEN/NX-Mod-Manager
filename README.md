<div align="center">
  <img src="./assets/icon.jpg" width="200"><br>

  <h1>NX Mod Manager</h1>
</div>

[中文](./README.md) [English](./README_EN.md)

一个专为Nintendo Switch设计的模组管理器，支持多语言，音效反馈，名称映射，提供直观的模组添加、安装、卸载和管理功能。

MOD采用复制安装，针对大体型的MOD安装速度较慢，但是考虑到方便备份MODS2文件夹，所以采用复制安装的方式。

插件支持本地缓存数据，20+系统除了首次扫描较慢，二次进入都可极速加载。

# 安装速度

**注意：从2.0版本开始，本插件仅支持安装ZIP类型MOD，手动安装方法不受影响**
**注意：文件安装功能不会删除只是不再更新和维护**

**注意：ZIP包内第一层级必须是`contents` 和 `exefs_patches`，否则无法识别。ZIP名字无所谓随便起。**
**注意：ZIP压缩用标准格式就行，仅支持ZIP格式，不支持7z\RAR格式。**

**以林可儿mod为例，2700+文件，500MB。**

文件方式安装时间1分-1分20秒，卸载时间15-20秒。

ZIP方式安装时间1分-1分20秒，卸载时间15-20秒。

**以公主之花mod为例，13805文件，1.15GB。**

文件方式安装时间7分10秒，卸载时间1分30秒。

ZIP方式安装时间4分50秒-5分03秒，卸载时间1分30秒。

## 使用方法

下载插件运行后，会自动创建mods2文件夹，用于存放MOD。自动创建/mods2/0000-add-mod-0000文件夹用来放待安装的ZIP类型MOD。

将ZIP类型MOD放入/mods2/0000-add-mod-0000文件夹中，打开插件，按功能提示即可正常使用。

**注意：新增的插件安装功能，仅支持ZIP类型MOD**

**注意：ZIP的名字只能为英文+数字**

**注意：ZIP包内第一层级必须是`contents` 和 `exefs_patches`，否则无法识别。**

更复杂但是批量安装更快的手动配置方法，可看后面的教程。

## 界面展示

![应用界面1](images/1.jpg)

![应用界面2](images/2.jpg)

![应用界面3](images/3.jpg)

![应用界面4](images/4.jpg)

![应用界面5](images/5.jpg)

![应用界面6](images/6.jpg)

![应用界面7](images/7.jpg)

### 完整目录结构图

以下是SD卡中模组的完整目录结构示例：

```
SD卡
└── mods2
    ├── game_name.json                                          # 游戏名称映射文件
    ├── [游戏名1][版本标签]
    │   └── [游戏ID]
    │       ├── mod_name.json                                   # 模组名称映射文件
    │       ├── [模组名称1][模组类型标签]
    │       │   ├── contents
    │       │   │   └── [具体模组内容]
    │       │   └── exefs_patches
    │       │       └── [具体模组内容]
    │       └── [模组名称2][模组类型标签]
    │           ├── contents
    │           │   └── [具体模组内容]
    │           └── exefs_patches
    │               └── [具体模组内容]
    │ 
    ├── [游戏名2][版本标签]
    │   └── [游戏ID]
    │       ├── mod_name.json                                    # 模组名称映射文件
    │       └── [模组名称][模组类型标签]
    │           └── MOD.ZIP                                      # 新增ZIP MOD格式支持
    │               └── contents ── [具体模组内容]
    │               └── exefs_patches
    │                       └── [具体模组内容]
    │ 
    └── [游戏名3][版本标签]
        └── [游戏ID]
            ├── mod_name.json                                    # 模组名称映射文件
            └── [模组名称][模组类型标签]
                ├── contents
                │   └── [具体模组内容]
                └── exefs_patches
                    └── [具体模组内容]
```

**目录结构说明**：
- 每个游戏可以有多个模组
- 每个模组可以同时包含`contents`和`exefs_patches`两种类型
- 新增ZIP MOD 格式支持，ZIP包放在MOD名字下，ZIP包内第一层级必须是`contents`和`exefs_patches`，否则无法识别

### 模组目录文字说明

模组使用目录：`SD卡/mods2`

完整的模组路径格式：
```
mods2/游戏名字[模组版本标签]/游戏ID/模组名字[模组类型标签]/具体模组文件
```

**重要说明**：
- 因为Switch文件系统不支持中文，所以整个路径全部用英文和数字，包括标点符号，否则无法识别
- 游戏ID路径下可放有多个模组，如：模组名字1[模组类型标签]，模组名字2[模组类型标签]
- 路径上若不标注标签，不影响实际使用，但建议标注，方便管理

### 标签格式说明

**版本标签格式**：`[版本号]`
- 模组版本标签请按实际模组适用版本填写，只填数字即可
- 例如：V1.2.0 写成 `[1.2.0]`

**模组类型标签**：
| 标签 | 含义     |
| ---- | -------- |
| [F]  | 帧率补丁 |
| [G]  | 图形增强 |
| [B]  | 游戏美化 |
| [P]  | 更多玩法 |
| [C]  | 金手指   |


**注意：** 标签格式为`[X]`，X为标签内容。为了避免识别出错，路径中非标签内容请不要使用`[]`括号。

### 模组类型示例

**Contents模组示例**：
```
Sentinels[1.0.0]/01008D7016438000/cheat code[C]/contents/01008D7016438000/cheats/F64F574.txt
```

**Exefs_patches模组示例**：
```
Bayonetta[1.2.0]/01004A4010FEA000/720&900[G]/exefs_patches/Bayo3/F00DF.ips
```

**新增ZIP MOD 格式支持**：
```
Sentinels[1.0.0]/01008D7016438000/cheat code[C]/MOD.ZIP
```

**注意**：ZIP包内第一层级必须是`contents`和`exefs_patches`，否则无法识别。ZIP名字无所谓随便起。

### 名称映射配置

#### 游戏名称映射

映射的名字会代替原本文件目录的名字显示在程序里。

**文件路径**：`SD/mods2/game_name.json` 

**格式**：
```json
{
  "游戏文件夹的名字": "想要映射的名字"
}
```

**示例**：
```json
{
  "Celeste[1.0.0]": "蔚蓝",
  "Bayonetta 3[1.2.0]": "贝姐3"
}
```

#### 模组名称映射

**文件路径**：`SD卡/mods2/游戏文件夹的名字[模组版本标签]/游戏ID/mod_name.json`

**格式**：
```json
{
  "模组文件夹的名字": {
    "display_name": "想要映射的名字",
    "description": "模组描述"
  }
}
```

**示例**：
```json
{
  "FPS-60[F]": {
    "display_name": "稳定30FPS",
    "description": "更细节的画面调整，帮助游戏稳定30FPS，提高游玩体验。"
  },
  "720&900[G]": {
    "display_name": "更多分辨率",
    "description": "将游戏分辨率调整为手持720p,底座900p，提高画面质量。"
  }
}
```

**注意**：编辑映射文件时，需注意花括号位置和JSON格式的正确性。

## 项目结构

```
SSM2/
├── src/                          # 源代码目录
│   ├── app.cpp/hpp               # 主应用程序类
│   ├── main.cpp                  # 程序入口点
│   ├── audio_manager.cpp/hpp     # 音效管理器
│   ├── lang_manager.cpp/hpp      # 多语言管理器
│   ├── async.hpp                 # 异步处理工具
│   ├── nanovg/                   # NanoVG图形库
│   ├── yyjson/                   # JSON解析库
│   └── utils/                    # 工具类
│       └── logger.cpp/hpp        # 日志系统
├── assets/                       # 资源文件
│   ├── icon.jpg                  # 应用图标
│   └── romfs/                    # RomFS资源
│       ├── lang/                 # 多语言文件
│       ├── *.jpg                 # 模组类型图标
│       └── shaders/              # 着色器文件
├── lib/                          # 第三方库
│   ├── switch-libpulsar/         # 音频库
│   └── libnxtc-add-version/      # 标题缓存库
├── Makefile                      # 构建配置
└── README.md                     # 项目说明
```

### 构建步骤

1. **克隆项目**:
   ```bash
   git clone <https://github.com/TOM-BadEN/NX-Mod-Manager.git>
   cd SSM2
   ```

2. **编译项目**:
   ```bash
   make
   ```

3. **输出文件**:
   - `NX-Mod-Manager.nro`: 可执行文件

## 致谢

感谢以下开源项目和库的贡献：
| 项目 | 链接 | 说明 |
| ---- | ---- | ---- |
| untitled | [ITotalJustice/untitled](https://github.com/ITotalJustice/untitled) | ITotalJustice 的 untitled |
| libhaze | [ITotalJustice/libhaze](https://github.com/ITotalJustice/libhaze) | ITotalJustice 的 libhaze |
| haze | [Atmosphere-NX/haze](https://github.com/Atmosphere-NX/Atmosphere/tree/master/troposphere/haze) | Atmosphere-NX 的 haze |
| devkitPro | [devkitPro](https://devkitpro.org/) | Nintendo Switch 开发工具链 |
| libnx | [switchbrew/libnx](https://github.com/switchbrew/libnx) | Switch 系统库，提供底层 API 支持 |
| deko3d | [devkitPro/deko3d](https://github.com/devkitPro/deko3d) | Switch GPU 图形 API，实现高性能渲染 |
| NanoVG | [memononen/nanovg](https://github.com/memononen/nanovg) | 轻量级 2D 矢量图形库 |
| switch-libpulsar | [p-sam/switch-libpulsar](https://github.com/p-sam/switch-libpulsar) | Switch 音频播放库 |
| fontstash | [memononen/fontstash](https://github.com/memononen/fontstash) | 字体渲染库 |
| yyjson | [ibireme/yyjson](https://github.com/ibireme/yyjson) | 高性能 JSON 解析库 |
| libnxtc | [DarkMatterCore/libnxtc](https://github.com/DarkMatterCore/libnxtc) | 标题缓存管理库（本项目使用的是添加了 version 成员的 libnxtc-add-version） |
| stb | [nothings/stb](https://github.com/nothings/stb) | 图像处理库 |
| miniz | [richgel999/miniz](https://github.com/richgel999/miniz) | 压缩库 |
