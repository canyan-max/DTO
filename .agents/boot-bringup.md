# BootLoad 升级入口接入与用户后续调整

2026-10-20。已接协议和维护流程，用户自行沿用旧Bootloader并调整APP链接/向量。
代理未修改Core、链接起始地址、scatter布局或Bootloader，也未烧录。

## 固定协议

- ASCII精确8字节：BootLoad；HEX：42 6F 6F 74 4C 6F 61 64。
- 大小写固定，无0x14前缀、CRC、CRLF或其他后缀。不匹配的业务帧沿原路由处理。
- DTO/HIK模式下UART1、485口1、485口2均可发起；沿用旧协议，无额外ACK。
- 公共app_local入口先识别BootLoad，再处理地址0x14的Modbus协议。
- 若模块配置、模式切换或其他维护正在占用通信，期间命令可被丢弃，结束后重发。

## 地址与标志

| 用途 | 地址／值 |
| --- | --- |
| 旧Bootloader及其代码区域 | 0x08000000～0x08002FFF，实际由用户Boot工程确定 |
| 升级标志专用页 | 0x08003000～0x080037FF，2KiB，第6页（0起算） |
| APP起点 | 0x08003800 |
| APP上限（不含） | 0x0801E000 |
| 建议APP长度 | 0x1A800（106KiB） |
| 请求升级标志 | 0x0000 |
| APP正常启动标志 | 0x55BB |
| 本机参数页 | 0x0801E000～0x0801E7FF，不被升级入口擦写 |
| 原备用页 | 0x0801E800～0x0801EFFF，不访问 |
| 继电器独立记忆页 | 0x0801F000～0x0801F7FF，不被升级入口擦写 |

地址常量统一在Config/boot_config.h。标志按旧代码进行64位编程，高位补0。
只允许写0或55BB，后端只擦标志页；重复同值不擦页。最终Boot端判断及升级擦除范围
还需用户核对，本实现不能约束未提供的Bootloader擦除APP时是否覆盖参数区域。

## 当前的布局保护

bsp_boot_layout_valid必须同时通过：

1. 实际Flash装载区域基址Load$$LR$$LR_IROM1$$Base＝0x08003800。
2. 装载区域末尾Load$$LR$$LR_IROM1$$Limit大于基址且不超过0x0801E000。
3. 当前SCB->VTOR＝0x08003800。

装载区域包括初始化RAM数据的Flash副本。当前主工程仍为0x08000000，因此检查失败：
启动不写55BB，收到BootLoad仅消耗并记失败，不写Flash、不复位、不作为DTO业务透传。
无额外开关需要开启，用户调整布局和向量后，检查自动通过。
非当前ARM Compiler 5工具链的基址查询默认回0，须在切换工具链时显式适配，不猜测布局。

## 用户需要调整的两处

- Keil Target的APP IROM起始地址改为0x08003800，长度改为0x1A800。
  如使用自定义scatter，需同步其LR/ER区域，保留LR_IROM1名称；确认map中的实际地址。
- 在APP早期初始化、启用相关中断之前，将向量表基址设为BOOT_APP_ADDRESS。
  可在Core/Src/main.c的USER CODE区域包含boot_config.h，并在适当的早期位置设置
  SCB->VTOR，随后执行__DSB()、__ISB()。本轮未代改生成代码。
  同时核对旧Boot跳转时留下的PRIMASK、NVIC、SysTick状态及APP恢复流程。

旧Program实际APP长度为0x1A800；不要直接照搬app-demo的0x1C000，后者覆盖当前存储预留范围。
调整后重新编译APP，不能仅把按0x08000000链接的旧HEX改下载位置。

## 正常启动与升级动作

- app_init先调用svc_boot_init。布局通过且标志低32位不是55BB时，尝试写55BB；
  已是55BB则不擦页。失败记录一次，不循环擦写，也不调用复位。
- BootLoad通过布局预检查后，app_local进入维护状态，不发送ACK。
- DTO暂停业务／HIK暂停采集和查询，等待三路已入队TX排空；任一路TX错误或
  超过20s事务时限都会取消，不写升级标志。
- 随后暂停三路RX、清理接收缓冲，svc_boot再次检查布局及TX空闲，写0并回读确认。
- 成功才调用bsp_system_reset（NVIC_SystemReset）；不执行运行中直接跳转。
- 写入或回读失败，恢复RX并退出，不复位、不自动重试；由外部重新发送。
- 不保存参数，不擦参数页或继电器页；引导复位后未保存的RAM参数/SN仍会按原规则丢失。

## 诊断与验证

svc_boot_get_status可读取startup_marks、requests、failures、layout_rejects；
服务内部g_boot_status保留同样数据。app_local的state/requests/failures记录事务状态。
当前直连地址构建下，BootLoad将增加布局拒绝统计，不应观察到复位。

新增test_boot.py：20个应用集成场景，覆盖两模式×三端口、三路TX排空、任何端口TX
错误取消、失败恢复与手动重发、原地址拒绝、启动55BB/同值不写、严格8字节、维护互斥、
提交前布局复核及原中断状态保持；13个实际BSP代码场景，在主机映射RAM模拟Flash，
验证只擦第6页、正常/升级标志、范围保护、解锁/擦除/编程/回读/加锁失败和相邻存储不变。
既有249场景回归通过，总计282场景通过；主工程0 error/0 warning。
Code=37678、RO=1030、RW=200、ZI=12016。未烧录，实际Bootloader闭环上板待用户验证。

完成地址/向量调整后，先检查APP经Boot正常启动，再从串口发8字节BootLoad，
观察标志0与系统复位，并确认进入旧升级程序；完成下载后APP应写回55BB。
验证升级保留参数/继电器记忆，并回归DTO/HIK业务。不要把软件模拟视为Boot硬件验收。

## 用户地址调整后的main接入（2026-10-20）

用户已设置IROM为0x08003800/0x1A800，并自行添加main的早期VTOR代码。
按用户本次明确要求，代理在main的USER Includes补boot_config.h、bsp_system.h，
恢复USER CODE 1中的__set_PRIMASK(0U)，位于向量设置/DSB/ISB之后、HAL_Init之前。
这是针对旧Boot关闭全局中断的启动恢复，不是临界区中的无条件开中断。
55BB仍由svc_boot维护，不再复制旧Flash代码；HAL_Init重新初始化SysTick。
其余生成区、链接配置及用户代码未改动。
本轮0 error/0 warning，map实际基址0x08003800、装载末尾0x0800D040，
Code=37746、RO=1030、RW=200、ZI=12016。此前“原地址构建/未改Core”是当时状态，
当前由本条替代。代理未烧录，Boot端外设/NVIC清理和跳转后的中断仍需上板验证。

## 引导上板反馈（2026-10-20）

用户明确确认“可以升级了，能沿用”，记录旧Bootloader与整合APP升级兼容上板通过。
此前APP链接/下载范围均为0x08003800、大小0x1A800，main向量及中断恢复已接入。
本次模式和触发端口未明确，不扩大为所有入口组合或参数/继电器升级保持均已验收。
代理此次仅更新验证记录，未执行烧录。
