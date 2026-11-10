#include "ISOTP.h"

static IsoTp_RxCallback_t g_rx_callback;

static uint8_t g_tx_buffer[ISOTP_MAX_PAYLOAD];
static uint16_t g_tx_length;
static uint16_t g_tx_position;
static uint8_t g_tx_sequence;
static uint8_t g_waiting_flow_control;

static uint8_t IsoTp_QueueFrame(const uint8_t data[8])
{
    Can_Frame_t frame = {0};

    frame.id = ISOTP_RESPONSE_ID;
    frame.dlc = 8U;
    memcpy(frame.data, data, 8U);

    return (CanTX_Enqueue(&frame) == pdPASS) ? 1U : 0U;
}

void IsoTp_Init(IsoTp_RxCallback_t callback)
{
    g_rx_callback = callback;
    g_waiting_flow_control = 0U;
}

uint8_t IsoTp_Send(const uint8_t *data, uint16_t length)
{
    uint8_t frame_data[8] = {0};

    if ((data == NULL) || (length == 0U) || (length > ISOTP_MAX_PAYLOAD))
    {
        return 0U;
    }

    if (length <= 7U)
    {
        frame_data[0] = (uint8_t)length;
        memcpy(&frame_data[1], data, length);

        return IsoTp_QueueFrame(frame_data);
    }

    if (g_waiting_flow_control != 0U)
    {
        return 0U;
    }

    memcpy(g_tx_buffer, data, length);
    g_tx_length = length;
    g_tx_position = 6U;
    g_tx_sequence = 1U;
    g_waiting_flow_control = 1U;

    frame_data[0] = 0x10U | (uint8_t)((length >> 8U) & 0x0FU);
    frame_data[1] = (uint8_t)length;
    memcpy(&frame_data[2], data, 6U);

    if (IsoTp_QueueFrame(frame_data) == 0U)
    {
        g_waiting_flow_control = 0U;
        return 0U;
    }

    return 1U;
}

static void IsoTp_SendConsecutiveFrames(void)
{
    while (g_tx_position < g_tx_length)
    {
        uint8_t frame_data[8] = {0};
        uint16_t remaining = g_tx_length - g_tx_position;
        uint8_t copy_length = (remaining > 7U) ? 7U : (uint8_t)remaining;

        frame_data[0] = 0x20U | g_tx_sequence;

        memcpy(&frame_data[1], &g_tx_buffer[g_tx_position], copy_length);

        if (IsoTp_QueueFrame(frame_data) == 0U)
        {
            g_waiting_flow_control = 0U;
            return;
        }

        g_tx_position += copy_length;
        g_tx_sequence = (uint8_t)((g_tx_sequence + 1U) & 0x0FU);
    }

    g_waiting_flow_control = 0U;
}

void IsoTp_OnCanFrame(const Can_Frame_t *frame)
{
    uint8_t pci_type;

    if ((frame == NULL) || (frame->id != ISOTP_REQUEST_ID) || (frame->dlc != 8U))
    {
        return;
    }

    pci_type = frame->data[0] >> 4U;

    if (pci_type == 0U)
    {
        uint8_t length = frame->data[0] & 0x0FU;

        if ((length > 0U) && (length <= 7U) && (g_rx_callback != NULL))
        {
            g_rx_callback(&frame->data[1], length);
        }
    }
    else if ((pci_type == 3U) && (frame->data[0] == 0x30U) && (g_waiting_flow_control != 0U))
    {
        IsoTp_SendConsecutiveFrames();
    }
}
