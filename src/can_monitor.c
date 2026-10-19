#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <poll.h>

#include "can_protocol.h" 
#include "crc8.h"

int main(int argc, char *argv[]) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    // 支持命令行传参切换网卡，默认使用 can0（例如 ./can_monitor vcan0）
    const char *ifname = (argc > 1) ? argv[1] : "can0";

    // 1. 创建原生套接字
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("[错误] Socket creation failed");
        return 1;
    }

    // 2. 获取接口索引并检查是否成功（极其关键！）
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("[错误] ioctl 获取网卡索引失败 (请检查 can0 是否已创建并插好)");
        close(s);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[错误] Bind failed");
        close(s);
        return 1;
    }

    // 3. 配置 poll() 结构体
    struct pollfd pfd;
    pfd.fd = s;
    pfd.events = POLLIN;

    struct can_frame frame;
    printf("Listening on %s (ifindex: %d)...\n", ifname, ifr.ifr_ifindex);
    fflush(stdout);

    while (1) {
        int ret = poll(&pfd, 1, 1000); 

        if (ret < 0) {
            perror("[错误] poll 异常");
            break;
        } else if (ret == 0) {
            // 1秒超时，打印提示并强制刷新输出缓冲区
            printf("[Poll] 1秒超时，当前总线无数据，CPU 可处理其他任务...\n");
            fflush(stdout);
        } else {
            // 如果触发了可读事件
            if (pfd.revents & POLLIN) {
                int nbytes = read(s, &frame, sizeof(struct can_frame));
                if (nbytes > 0) {
                    // 使用 CAN_SFF_MASK 屏蔽最高位的标志位，提取纯正的 11 位标准 ID
                    uint32_t id = frame.can_id & CAN_SFF_MASK;
                    printf("\n--- 收到 CAN ID: 0x%03X (DLC: %d) ---\n", id, frame.can_dlc);
                    
                    uint8_t rx_crc = frame.data[7];
                    uint8_t calc_crc = Calculate_CRC8_J1850(frame.data, 7);
                    printf("[CRC 校验] 接收: 0x%02X | 计算: 0x%02X -> %s\n", 
                           rx_crc, calc_crc, (rx_crc == calc_crc) ? "通过" : "失败");

                    if (id == 0x180) {
                        SensorMsg_t decoded;
                        Unpack_Msg_0x180(frame.data, &decoded);
                        printf("[0x180 传感器状态] Temp: %d | Hum: %d | Mode: %d | AppValue: %d | Valid: %d\n", 
                               decoded.temperature, decoded.humidity, decoded.current_mode, decoded.app_value, decoded.valid_flag);
                        printf("[状态诊断] Rolling Counter: %d | Fault Bits: 0x%02X\n", 
                               decoded.rx_counter, decoded.fault_bits);
                    } 
                    else if (id == 0x200) {
                        ControlCmd_t cmd;
                        Unpack_Msg_0x200(frame.data, &cmd);
                        printf("[0x200 控制指令] Target Mode: %d | Target Val: %d | Rolling Counter: %d\n",
                               cmd.target_mode, cmd.target_value, cmd.rx_counter);
                    }
                    else if (id == 0x280) {
                        printf("[0x280 应答/告警] Status/Cmd: 0x%02X | 关联 Counter: %d | Mode: %d\n", 
                               frame.data[0], frame.data[1], frame.data[2]);
                    }
                    else {
                        printf("[0x%03X 其他/心跳帧] Raw: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                               id, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                               frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
                    }
                    fflush(stdout);
                }
            } 
            // 如果网卡未 UP 或物理断开导致异常事件，防止无限死循环空转
            else if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                printf("[警告] 网卡状态异常 (revents=0x%X)，请检查 %s 是否已 UP！\n", pfd.revents, ifname);
                sleep(1); 
            }
        }
    }

    close(s);
    return 0;
}