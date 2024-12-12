# 编译器设置  
CC = gcc  
CFLAGS = -Wall -Wextra -Werror -g -O2  
LDFLAGS =  

# 目录设置  
SRC_DIR = .  
OBJ_DIR = obj  
BIN_DIR = bin  
INC_DIR = .  

# 源文件和目标文件  
SRCS = $(wildcard $(SRC_DIR)/*.c)  
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)  
DEPS = $(OBJS:.o=.d)  
TARGET = $(BIN_DIR)/server  

# 版本信息  
VERSION = 1.0.0  

# 确保目标目录存在  
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))  

# 默认目标  
all: $(TARGET)  

# 链接目标文件生成可执行文件  
$(TARGET): $(OBJS)  
	@echo "Linking $@..."  
	@$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)  
	@echo "Build complete!"  

# 编译源文件生成目标文件  
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c  
	@echo "Compiling $<..."  
	@$(CC) $(CFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@  

# 包含依赖文件  
-include $(DEPS)  

# 清理生成的文件  
clean:  
	@echo "Cleaning build files..."  
	@rm -rf $(OBJ_DIR) $(BIN_DIR)  
	@echo "Clean complete!"  

# 创建必要的目录结构  
init:  
	@echo "Creating directory structure..."  
	@mkdir -p ../html ../logs  
	@echo "Directory structure created!"  

# 运行服务器  
run: $(TARGET)  
	@echo "Starting server..."  
	@$(TARGET)  

# 调试  
debug: CFLAGS += -DDEBUG  
debug: clean $(TARGET)  
	@echo "Starting GDB..."  
	@gdb $(TARGET)  

# 显示帮助信息  
help:  
	@echo "Available targets:"  
	@echo "  make        - Build the server"  
	@echo "  make clean  - Remove build files"  
	@echo "  make init   - Create directory structure"  
	@echo "  make run    - Run the server"  
	@echo "  make debug  - Debug the server"  
	@echo "  make help   - Show this help message"  

# 显示版本信息  
version:  
	@echo "Simple HTTP Server version $(VERSION)"  

# 检查内存泄漏  
memcheck: $(TARGET)  
	valgrind --leak-check=full --show-leak-kinds=all $(TARGET)  

# 声明伪目标  
.PHONY: all clean init run debug help version memcheck  

# 设置默认目标  
.DEFAULT_GOAL := all