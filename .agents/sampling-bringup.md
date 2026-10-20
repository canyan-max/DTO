# 非 DMA ADC 上板测试

后续进度：用户确认电压精度够用；DTO 已接欠压覆盖，见
[DTO 收尾测试](dto-relay-bringup.md)。下文“本轮不接欠压”等描述为采样首批范围。

## 当前实现

- Bsp/bsp_adc 封装 PA0 / ADC1 通道 0，启动前停止、配置单次转换并校准。
  不使用 DMA、ADC 中断或新增定时器中断。
- Core/adc.c 与 IOC 保持生成配置；BSP 在启动阶段将连续转换切换为单次。
- Services/svc_sampling 每 10 ms 最多启动一次转换，下一次主循环检查 EOC；
  EOC 未就绪时立即返回。未完成达到 5 ms 则记录超时，停止并延后恢复。
- 16 个成功样本为一组，整组平均后发布结果，约 160 ms 更新一次。
  初次凑满前 valid=0；失败或样本超过 1 s 未更新时失效并清空部分滤波。
  失败后每 1 s 尝试重新初始化；错误不终止主循环或串口通信。
- 用户确认沿用 Program：先 raw*3300/4095 得到引脚 mV，再乘
  107500/7500 得到输入 mV。两次整数除法保持原截断，无 +200 mV 补偿，
  不测 VREFINT，准确度仍受实际 VDDA、分压电阻等影响。
- 参数见 Config/sampling_config.h。本轮仅采样和观测，不接欠压控制，
  不改变继电器、电源状态，不增加协议寄存器。

## 操作

1. 手动下载最新 MDK-ARM/comm_convert/comm_convert.hex 并运行。
2. Keil Watch 查看 Services/svc_sampling.c 的文件作用域变量 g_sampling。
   若名称解析冲突，可在源码中选中该变量后添加到 Watch。
3. 上电约 0.2 s 后查看以下字段，至少运行数秒：

| 字段 | 预期 |
| --- | --- |
| initialized | 1 |
| valid | 凑满 16 点后为 1 |
| raw / filtered_raw | 0～4095，前者单点、后者整组均值 |
| adc_pin_mv | PA0 引脚电压，mV |
| input_mv | 分压前输入电压，mV；例如 12000 代表 12 V |
| samples / updates | 持续增长，约 100 点/s、6 次发布/s |
| errors / timeouts | 正常运行应为 0 |

4. 用万用表记录输入端电压，并与 input_mv 比较；必要时同时测 PA0 电压。
   先提供这些实测数据，再决定是否调整标称参考或补偿。
5. 同时复测 DTO 和参数读取，确认采样期间通信仍正常。

建议观察时让 MCU 持续运行；长时间断点暂停会改变调度和看门狗行为。
此阶段的电压只用于观测，即使 uv_enabled=1 也尚未驱动保护动作。

## 软件验证

- test_sampling.py 使用实际 ADC BSP 与采样服务、受控假 HAL，通过 12 场景：
  调度/EOC 非阻塞、完整分组平均、原版换算/满量程、超时重试、过期失效、
  校准和启动失败、溢出、越界原始值、计时回绕、停止失败、HAL 读取失败、
  BSP 参数/配置防护。
- 原 53 个通信与参数场景全部通过；这些回归隔离模拟 ADC 服务，
  ADC 自身由上述 12 场景验证，整机并行效果仍需上板确认。
- ARMCC 0 error / 0 warning：Code=27064、RO=900、RW=80、ZI=11016，
  总 ROM=28044 字节，总 RW/ZI=11096 字节。代理仅构建，未烧录。
- 2026-10-19 用户确认“可行”，截图 raw/filtered_raw=1063，adc_pin_mv=856，
  input_mv=12269，samples=1169、updates=73，valid/initialized=1，
  errors/timeouts=0。基本采样上板通过；万用表精度比对及采样同时 DTO 通信
  尚未单独反馈，不把本次截图扩大为这些项目已通过。
