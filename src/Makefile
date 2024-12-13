# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra

# 目录设置
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# 源文件和目标文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/program

# 伪目标声明
.PHONY: all clean mkdirs

# 默认目标
all: mkdirs $(TARGET)

# 链接规则
$(TARGET): $(OBJS)
	@echo "Linking $@..."
	@$(CC) $^ -o $@

# 编译规则
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# 创建目录
mkdirs:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# 清理规则
clean:
	@echo "Cleaning..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)