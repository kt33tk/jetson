CC = gcc
CFLAGS = -Iinclude -Wall -Wextra

# 默认目标
all: build/can_monitor

# 编译所有 .c 文件到 build 目录下的 .o 文件
build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 链接所有 .o 文件生成最终可执行文件
build/can_monitor: build/main.o build/can_protocol.o build/crc8.o
	$(CC) $^ -o $@

# 清理构建残留
clean:
	rm -f build/*.o build/can_monitor

# 编译并运行
run: all
	./build/can_monitor

# 测试目标
test: all
	@echo "Test suite placeholder..."