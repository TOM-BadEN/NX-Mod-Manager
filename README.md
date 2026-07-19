<div align="center">
  <img src="./.github/forReadme/icon.png" width="200"><br>

  <h1>NX Mod Manager</h1>

[![platform](https://img.shields.io/badge/bilibili-教程-FB7299?logo=bilibili&logoColor=FFFFFF)](https://www.bilibili.com/video/BV1zd5o6wE5B/)
[![Latest Version](https://img.shields.io/github/v/release/TOM-BadEN/NX-Mod-Manager?label=Latest&color=blue&logo=github&logoColor=FFFFFF)](https://github.com/TOM-BadEN/NX-Mod-Manager/releases/latest)
[![GitHub Downloads](https://img.shields.io/github/downloads/TOM-BadEN/NX-Mod-Manager/total?label=Downloads&color=6f42c1&logo=github&logoColor=FFFFFF)](https://somsubhra.github.io/github-release-stats/?username=TOM-BadEN&repository=NX-Mod-Manager&page=1&per_page=300)
[![HB App Store](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/TOM-BadEN/NX-Mod-Manager/main/.github/forReadme/hbappstore.json&label=HB%20store&color=green&logo=homeassistantcommunitystore&logoColor=FFFFFF)](https://hb-app.store/switch/NXModManager)
[![PayPal](https://img.shields.io/badge/PayPal-Donate-blue?logo=paypal&logoColor=FFFFFF)](https://PayPal.me/TomSun666)
[![微信](https://img.shields.io/badge/微信-捐赠-07C160?logo=wechat&logoColor=FFFFFF)](https://github.com/TOM-BadEN/NX-Mod-Manager/blob/main/.github/forReadme/WeChat%20Pay.png)

<p><a href="./README.md">中文</a>&nbsp;&nbsp;&nbsp;&nbsp;<a href="./README_EN.md">English</a></p>

</div>

## 项目简介

&emsp;&emsp;一款专为 Nintendo Switch 平台设计的模组管理工具。当前版本已完成全量代码重写与底层架构重构，在功能与稳定性方面均有显著提升。项目完全开源且永久免费，不包含任何形式的付费内容。支持在线浏览与下载模组，并提供完整的设备内管理流程，可在无需手动操作 SD 卡的情况下完成模组的获取与管理。在线模组资源的建设与维护依赖社区用户的共同参与与贡献。

## 核心功能

> [!NOTE]
> &emsp;&emsp;v3.2.7 版本已添加对《怪物猎人：崛起》的支持。<br>
> &emsp;&emsp;v3.3.0 版本已添加对《饥荒》的支持。

<div align="center">

| 功能 | 说明 |
| --- | --- |
| 游戏管理 | 支持从已安装游戏列表或手动输入 TID 添加游戏，支持游戏的移除与收藏 |
| 模组管理 | 支持模组的添加、安装、卸载与移除，兼容 ZIP 压缩包与文件夹两种模组格式 |
| 补丁转换 | 安装模组时自动将 pchtxt 文本补丁转换为 IPS 补丁 |
| 智能检测 | 自动识别非标准目录结构的模组文件，并检测多模组之间的文件冲突 |
| 智能搜索 | 支持拼音、多音字、首字母、模糊匹配等多种搜索方式，深度适配中文用户 |
| 模组开关 | 支持一键禁用或恢复全部模组，便于快速排查游戏问题 |
| 文件传输 | 支持 MTP（USB 有线）与 FTP（Wi-Fi 无线）两种方式，将模组传输至主机 |
| 在线商店 | 内置模组商店，支持模组的浏览、搜索、下载与上传，并提供点赞与留言功能 |
| 自定义内容 | 支持自定义多项内容，如游戏名称、模组名称、描述、版本号、作者及类型等信息 |
| 多语言支持 | 提供简体中文、繁体中文与英文界面 |
| 主题切换 | 支持浅色与深色主题，并可跟随系统设置自动切换 |
| 自动更新 | 支持在应用内检查并下载新版本 |
| 强制清理 | 用于修复异常中断导致的引用计数损坏问题 |

</div>

## 用户须知

> [!WARNING]
> &emsp;&emsp;当前版本进行了全量代码重写与底层架构的重新设计，由于架构变动较大，旧版本数据难以兼容迁移，与 2.x 版本不兼容，升级后需重新配置。并且由于 Switch 平台模组机制的特殊性，各模组之间无法完全独立运行，任何管理器都无法规避来自外部操作的影响。在使用本管理器期间，手动操作 SD 卡或混用其他管理器进行模组的安装与卸载，都可能导致内部数据失效，在卸载时出现文件残留。建议在使用前，先将此前通过手动方式、2.x 版本管理器或其他第三方管理器安装的模组完全卸载，以确保环境干净。

> [!CAUTION]
> &emsp;&emsp;本项目由个人独立维护，作者并非专业开发者，无法保证软件不存在缺陷。使用过程中如出现游戏存档损坏或模组文件损坏等问题，相关风险需由使用者自行承担。服务器与网站同样由作者一人维护，无法保证长期稳定运营，但承诺本项目始终不会收取任何费用。如未来无法继续维护，将会把服务器及相关数据移交给愿意接手的维护者。对于用户上传的模组内容，如存在侵犯原作者合法权益的情况，请及时联系作者，相关内容将在核实后立即删除处理。

> [!NOTE]
> &emsp;&emsp;由于个人精力有限，无法独立完成大量模组资源的收集与上传工作，在线模组商店的内容建设依赖社区用户的共同参与与维护。如您拥有优质模组资源，欢迎通过网页端的上传功能分享给其他用户。上传前请优先获得原作者授权许可，若资源来源于他人发布的帖子，请在上传时附带相关原始链接以便溯源。严禁上传收费、色情或其他违规及敏感内容，一经发现将立即删除，并可能对相关账号进行永久封禁处理。

## 界面展示

<div align="center">
  <img src="./.github/forReadme/home.jpg" width="720"><br>
  <img src="./.github/forReadme/mod.jpg" width="720"><br>
  <img src="./.github/forReadme/storeList.jpg" width="720"><br>
  <img src="./.github/forReadme/store.jpg" width="720">
</div>

## 使用方法

&emsp;&emsp;请[点击此处](https://github.com/TOM-BadEN/NX-Mod-Manager/releases/latest)下载最新的 nro 文件，并将其安装至 SD 卡中。软件的具体使用方法请[点击此处](https://www.bilibili.com/video/BV1zd5o6wE5B/)查看视频教程，或在应用内通过"关于软件"查看基础教程。

## 反馈渠道

&emsp;&emsp;请优先通过 GitHub 的 [Issue](https://github.com/TOM-BadEN/NX-Mod-Manager/issues) 提交反馈，也可以在应用内通过"关于软件"获取其他反馈渠道。无论使用哪种方式，请尽可能提供完整且准确的信息。受限于个人精力，对于信息不完整或描述模糊的反馈，可能无法进行反复追问与补充处理，敬请理解。建议在反馈中一并提供大气层及系统固件版本号、问题触发的详细操作步骤、问题是否可稳定复现、如仅在特定文件触发请提供相关文件，以及其他有助于问题定位的信息。

## 编译说明

**环境要求：**
- devkitPro
- Ninja

**编译指令：**

```bash
make          # 编译项目
make debug    # 调试编译 + nxlink 发送
make clean    # 清理构建目录
```

## 特殊说明

> [!CAUTION]
> &emsp;&emsp;服务器由作者个人承担维护，且该文件包含所有后端 API 的接口地址，运营成本高昂且抗压能力有限，公开接口地址将面临滥用与恶意攻击的风险。因此，本项目中的 [`url.hpp`](https://github.com/TOM-BadEN/NX-Mod-Manager/blob/main/code/include/api/url.hpp.example) 文件未纳入开源范围。项目可直接编译使用，但在线相关功能将不可用。如需完整的网络功能，请自行搭建后端服务并填入对应的接口地址。

## 致谢

感谢以下开源项目的支持：

<div align="center">

| 项目 | 说明 | 作者 |
| --- | --- | --- |
| [borealis](https://github.com/xfangfang/borealis) | UI 框架 | [xfangfang](https://github.com/xfangfang) [natinusala](https://github.com/natinusala)|
| [libhaze](https://github.com/Atmosphere-NX/Atmosphere/tree/master/troposphere/haze) | MTP | [ITotalJustice](https://github.com/ITotalJustice) |
| [ftpsrv](https://github.com/ITotalJustice/ftpsrv) | FTP | [ITotalJustice](https://github.com/ITotalJustice) |
| [yyjson](https://github.com/ibireme/yyjson) | 高性能 JSON 解析 | [ibireme](https://github.com/ibireme) |
| [cpp-pinyin](https://github.com/wolfgitpr/cpp-pinyin) | 拼音搜索 | [wolfgitpr](https://github.com/wolfgitpr) |
| [QR-Code-generator](https://github.com/nayuki/QR-Code-generator) | 二维码生成 | [nayuki](https://github.com/nayuki) |
| [miniz](https://github.com/richgel999/miniz) | ZIP 压缩与解压 | [richgel999](https://github.com/richgel999) |
| [libnxtc](https://github.com/DarkMatterCore/libnxtc) | 游戏信息缓存 | [DarkMatterCore](https://github.com/DarkMatterCore) |
| \ | 问题帮助 | [masagrator](https://github.com/masagrator) |
| \ | 提供了数百个个人原创 MOD | [明月清风](https://www.tekqart.com/space-uid-2878778.html) |
| \ | 提供了大量 NS 游戏信息 | [时鹏亮](https://shipengliang.com/about-us) |

</div>

## 开源许可

本项目基于 [GPL-2.0](LICENSE) 许可证开源。
