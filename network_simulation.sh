#!/bin/bash

# 网络接口（根据实际情况修改）
INTERFACE="lo"

# 设置 LAN 环境
set_lan() {
    echo "Setting LAN environment (256Mbps bandwidth, adding 2ms RTT)..."
    sudo tc qdisc add dev $INTERFACE root netem rate 256mbit delay 1ms
}

# 设置 WAN 环境
set_wan() {
    echo "Setting WAN environment (100Mbps bandwidth, 20ms RTT)..."
    sudo tc qdisc add dev $INTERFACE root netem rate 100mbit delay 10ms
}

# 恢复网络状态
restore_network() {
    echo "Restoring network to initial state..."
    sudo tc qdisc del dev $INTERFACE root
}

# 执行任务：这里调用另一个脚本，并传递测试文件名称
do_test() {
    local test_file=$1
    echo "Executing multiparty test with test file: $test_file..."
    ./run_multiparty_test.sh "$test_file"
}

# 主流程
main() {
    if [ "$1" == "LAN" ]; then
        # 设置 LAN 环境
        set_lan
    elif [ "$1" == "WAN" ]; then
        # 设置 WAN 环境
        set_wan
    else
        echo "Invalid network type. Please choose either 'LAN' or 'WAN'."
        exit 1
    fi

    if [ -z "$2" ]; then
        echo "Error: Test file name is required as the second argument."
        exit 1
    fi

    # 执行任务
    do_test "$2"
    
    # 恢复网络环境
    restore_network
    echo "Network restored to initial state."
}

# 捕捉 Ctrl+C 或脚本中断，确保网络恢复
trap 'echo "Script interrupted. Restoring network..."; restore_network' INT

# 执行主流程
main "$1" "$2"