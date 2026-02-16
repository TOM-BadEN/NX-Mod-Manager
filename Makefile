# ============================================================================
# Switch 构建脚本
# ============================================================================
# 使用方法:
#   make        - 编译项目 (deko3d 渲染器)
#   make debug  - 调试编译 + nxlink 发送
#   make clean  - 清理构建目录
# ============================================================================

.PHONY: all debug clean

# Switch IP 地址（nxlink 用）
SWITCH_IP ?= 192.168.50.113

# 构建目录
BUILD_DIR = build_switch_D3D

# 自动从 CMakeLists.txt 读取项目名称
PROJECT_NAME = $(shell grep "^project(" CMakeLists.txt | sed 's/project(\([^)]*\)).*/\1/')

# 编译
all:
	cmake -B $(BUILD_DIR) -G Ninja
	ninja -C $(BUILD_DIR) $(PROJECT_NAME).nro && echo -e "\033[32m编译成功！\n软件包体积：$$(du -h $(BUILD_DIR)/$(PROJECT_NAME).nro | cut -f1)\033[0m" || (echo -e "\033[31m编译失败\033[0m" && false)

# 调试编译 + nxlink 发送
debug:
	cmake -B $(BUILD_DIR) -G Ninja -DNXLINK=ON
	ninja -C $(BUILD_DIR) $(PROJECT_NAME).nro && echo -e "\033[32m编译成功！\n软件包体积：$$(du -h $(BUILD_DIR)/$(PROJECT_NAME).nro | cut -f1)\033[0m" || (echo -e "\033[31m编译失败\033[0m" && false)
	/opt/devkitpro/tools/bin/nxlink -s -a $(SWITCH_IP) $(BUILD_DIR)/$(PROJECT_NAME).nro; true

# 清理
clean:
	rm -rf $(BUILD_DIR)
