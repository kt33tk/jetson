#include "test.h"

void Test_Task(void *pvParameters)
{
		ControlContext_t ctx;
	    Sensor_Sample_t sensor = {0};
	    ControlCommand_t cmd = {0};
	    uint32_t current_time = 0;

	    printf("开始运行自动化测试...\n");

	    // ==========================================
	    // 用例 1：上电保持 INIT，连续 3 个有效样本后进入 AUTO
	    // ==========================================
	    ControlCore_Init(&ctx, current_time);
	    assert(ctx.current_mode == CTRL_MODE_INIT);

	    sensor.valid_flag = 1;
	    sensor.temperature = 25;

	    // 第 1 个数据
	    current_time += 1000;
	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_INIT); // 还没满 3 次，必须保持 INIT

	    // 第 2 个数据
	    current_time += 1000;
	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_INIT);

	    // 第 3 个数据
	    current_time += 1000;
	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_AUTO); // 满 3 次，必须切入 AUTO

	    // ==========================================
	    // 用例 2：INIT 8 秒无有效数据后进入 SAFE
	    // ==========================================
	    ControlCore_Init(&ctx, current_time); // 重置系统
//	    sensor.valid_flag = 0;                // 模拟数据无效
	    current_time += 8001;                 // 瞬间流逝 8.001 秒
	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_SAFE);
	    assert(ctx.fault_bits & 0x02);        // 必须包含 INIT 超时故障码

	    // ==========================================
	    // 用例 3：AUTO -> MANUAL 正常切换与超时测试
	    // ==========================================
	    // (假设当前已经在 AUTO 状态)
	    ctx.current_mode = CTRL_MODE_AUTO;

	    // 发送切入手动的命令
	    cmd.has_new_cmd = 1;
	    cmd.target_mode = CTRL_MODE_MANUAL;
	    cmd.manual_request = 1; // 手动请求开启
	    cmd.cmd_timestamp = current_time;

	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_MANUAL);
	    assert(ctx.app_value == 1); // 必须听从手动开启命令

	    // 模拟 2.001 秒没有新命令
	    current_time += 2001;
	    ControlCore_Step(&ctx, &sensor, &cmd, current_time);
	    assert(ctx.current_mode == CTRL_MODE_SAFE); // 必须因为超时掉入 SAFE
	    assert(ctx.app_value == 0);                 // SAFE 状态应用值必须强制关闭

	    printf("自动化测试运行成功...\n");

	for(;;)
	{

	}
}
