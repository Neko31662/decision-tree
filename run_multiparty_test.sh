#!/bin/bash


if [ $# -lt 1 ]; then
    echo "Usage: $0 <test_program_name>"
    exit 1
fi

TEST_PROG_NAME=$1
BUILD_DIR="./build"
TEST_PROG="$BUILD_DIR/$TEST_PROG_NAME"
NUM_PARTIES=3

# 检查程序是否存在
if [ ! -f "$TEST_PROG" ]; then
    echo "Error: $TEST_PROG not found. Please build it first."
    echo "Run: cd build && cmake .. && make $TEST_PROG_NAME"
    exit 1
fi

echo "========================================"
echo "Multi-Party Test Runner"
echo "Test Program: $TEST_PROG_NAME"
echo "Number of parties: $NUM_PARTIES"
echo "========================================"
echo ""

# 清理之前的日志和PID文件
rm -f party*.log party*.pid

# 存储所有进程的PID
PIDS=()

# 启动所有进程
for i in $(seq 0 $((NUM_PARTIES - 1))); do
    echo "Starting Party $i..."
    $TEST_PROG $i > party$i.log 2>&1 &
    PID=$!
    PIDS+=($PID)
    echo $PID > party$i.pid
    echo "  Party $i started with PID: $PID"
done

echo ""
echo "All parties started. PIDs: ${PIDS[@]}"
echo "Waiting for all parties to complete..."
echo ""

# 等待所有进程完成
EXIT_CODES=()
for i in $(seq 0 $((NUM_PARTIES - 1))); do
    wait ${PIDS[$i]}
    EXIT_CODE=$?
    EXIT_CODES+=($EXIT_CODE)
done

echo "========================================"
echo "Test Execution Completed"
echo "========================================"

# 显示所有进程的退出码
ALL_SUCCESS=true
for i in $(seq 0 $((NUM_PARTIES - 1))); do
    echo "Party $i exit code: ${EXIT_CODES[$i]}"
    if [ ${EXIT_CODES[$i]} -ne 0 ]; then
        ALL_SUCCESS=false
    fi
done

echo ""
echo "========================================"
echo "Party Logs"
echo "========================================"

# 显示所有日志
for i in $(seq 0 $((NUM_PARTIES - 1))); do
    echo ""
    echo "=== Party $i Log ==="
    if [ -f "party$i.log" ]; then
        cat party$i.log
    else
        echo "Log file not found!"
    fi
done

echo ""
echo "========================================"

# 检查是否所有进程都成功
if $ALL_SUCCESS; then
    echo "✓ All parties completed successfully!"
    echo "========================================"
    
    # 清理日志和PID文件
    # rm -f party*.log party*.pid
    
    exit 0
else
    echo "✗ Some parties failed. Check logs above."
    echo "========================================"
    echo ""
    echo "Log files preserved for debugging:"
    ls -lh party*.log 2>/dev/null
    
    exit 1
fi
