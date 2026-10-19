#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "can_protocol.h"
#include "crc8.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("用法: %s <mode: 0-手动/1-自动/2-清故障> <value> <counter: 0~15>\n", argv[0]);
        printf("示例: %s 1 260 1\n", argv[0]);
        return 1;
    }

    uint8_t mode = (uint8_t)atoi(argv[1]);
    uint16_t val = (uint16_t)atoi(argv[2]);
    uint8_t cnt  = (uint8_t)atoi(argv[3]);

    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    strcpy(ifr.ifr_name, "can0");
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl failed (请检查 can0 是否存在)");
        close(s);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(s);
        return 1;
    }

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x200;
    frame.can_dlc = 8;

    // 组装 0x200 控制指令并计算 CRC
    ControlCmd_t cmd = {0};
    cmd.target_mode = mode;
    cmd.target_value = val;
    cmd.rx_counter = cnt & 0x0F;

    Pack_Msg_0x200(&cmd, frame.data);

    if (write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write failed");
        close(s);
        return 1;
    }

    printf("[TX 0x200] Mode:%d | Val:%d | Counter:%d | CRC:0x%02X\n",
           mode, val, cmd.rx_counter, frame.data[7]);

    close(s);
    return 0;
}