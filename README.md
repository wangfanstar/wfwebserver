# Simple HTTP Server  

一个简单的 HTTP 服务器实现，支持基本的静态文件服务和日志记录功能。  

## 功能特点  

- 基本的 HTTP GET 请求处理  
- 静态文件服务  
- 自动索引页面支持  
- 轮转日志系统  
  - 最多保存10个日志文件  
  - 单个日志文件最大100MB  
  - 总日志大小限制1GB  
  - 自动日志轮转  
- 可配置的服务端口  

## 系统要求  

- Linux/Unix 操作系统  
- GCC 编译器  
- Make 工具  
- 至少1GB磁盘空间（用于日志）  

## 目录结构  

```plaintext  
project/  
├── src/  
│   ├── server.c      # 服务器主程序  
│   ├── log.c         # 日志系统实现  
│   ├── log.h         # 日志系统头文件  
│   └── Makefile      # 编译脚本  
├── bin/              # 编译后的可执行文件目录  
├── obj/              # 编译过程中的目标文件目录  
├── html/             # 网站根目录  
│   └── index.html    # 默认首页  
└── logs/             # 日志文件目录  
    └── server_1.log  # 日志文件  
```

# 编译和安装

## 克隆仓库：
```bash
git clone https://github.com/yourusername/simple-http-server.git  
cd simple-http-server  
```

## 创建必要的目录结构：
```bash
make init  
```

## 编译项目：
make  

## 运行服务器

### 基本运行
使用默认端口(8081)运行：

```bash
make run  
```

### 指定端口运行
```bash
./bin/server 8080  
```

# 配置说明
服务器的主要配置在源代码中定义：

```bash
DEFAULT_PORT: 默认服务端口(8081)
DEFAULT_ROOT: 网站根目录("../html")
LOG_DIR: 日志目录("../logs")
MAX_LOG_FILES: 最大日志文件数(10)
MAX_LOG_SIZE: 单个日志文件大小限制(100MB)
MAX_TOTAL_SIZE: 总日志大小限制(1GB)
```

# 日志系统
服务器使用轮转日志系统记录运行信息：

访问日志：记录所有HTTP请求
错误日志：记录服务器错误
系统日志：记录服务器启动、关闭等系统事件
日志文件格式：

```ruby
[YYYY-MM-DD HH:MM:SS] 日志内容  
```
# 开发工具
Makefile 提供了多个开发辅助命令：

```bash
make        # 编译项目  
make clean  # 清理编译文件  
make init   # 创建目录结构  
make run    # 运行服务器  
make debug  # 调试模式运行  
make help   # 显示帮助信息  
make memcheck # 内存泄漏检查  
```

# 调试
使用 GDB 调试：

```bash
make debug 
```
 
## 内存泄漏检查：

```bash
make memcheck  
```

# 常见问题
## 端口被占用
```makefile
错误信息: "Bind failed: Address already in use"  
```
解决方案: 更换端口或关闭占用端口的程序  

## 权限问题
```makefile
错误信息: "Permission denied"  
```
解决方案: 确保有适当的目录访问权限  

# 性能考虑
服务器使用单进程模型
适合小型站点和开发环境
建议并发连接数不超过100
建议静态文件大小不超过100MB

# 安全注意事项
仅支持基本的静态文件服务
不支持 HTTPS
建议仅在受信任的网络环境中使用
不建议在生产环境中使用

# 贡献指南
Fork 项目
创建特性分支
提交更改
推送到分支
创建 Pull Request

# 许可证
MIT License

作者
wangfanster

版本历史
v1.0.0 (2024-12-12)
初始版本
基本的 HTTP 服务功能
日志系统实现


[相关项目或贡献者]
联系方式
Email: wangfanstar@163.com
GitHub: @wangfanstar