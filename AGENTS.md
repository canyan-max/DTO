# AGENTS.md

## 开发前必读

- 当前工程/Git根目录是Application；先读[项目交接记录](.agents/project-memory.md)。
  该文件只保存当前事实、验证边界和待办；历史从Git查询，不再重复堆叠阶段日志。
- 同级Bootload由用户维护，未明确要求不查看、不修改；旧Program/app-demo不在本仓库中。
- Windows始终使用PowerShell 7：`C:/Program Files/PowerShell/7/pwsh.exe`，不使用powershell.exe。
- 保留用户改动，只修改当前需求范围；计划和讨论不得写成已实现/已验收。

## 架构和实现规则

- Core保留CubeMX初始化/中断入口，优先仅改USER CODE区；不批量格式化生成代码。
- Bsp封装硬件/HAL；Services提供公共服务；Protocols只编解码；App负责模式和业务；Config放配置。
- Drivers为HAL/CMSIS，原则上保持原样；必要改动说明原因与影响。不要将旧参考工程整套加入构建。
- 裸机、静态分配，不使用malloc/free，不引入RTOS。校验指针、长度、索引、容量和超时。
- 中断只收发/搬运/标记，不执行协议业务、阻塞等待、延时或格式化日志。
- 临界区短小，保存并恢复原中断状态；main中恢复旧Boot关闭中断的明确交接流程除外。
- UART TC完成才能释放发送缓冲；两路485回复独立排队，UART1串行输出。
- 每口仅一个RX消费者，由App分发到服务；下层不得反向依赖App。
- 参数、继电器/欠压和RAM SN共用服务，模式切换不得重新初始化或复制状态。
- 不猜测引脚极性、设备身份、采样换算、协议、波特率或Flash布局；不要更改已确认值。
- HAL类型只进入Core/Bsp或硬件绑定配置；每类HAL回调仅一个实现，统一分发。

## 必须保持的当前约定

- STM32G070KBT6，ARMCC5.06u6；SYSCLK64MHz，CPU/HCLK16MHz。主工程为MDK-ARM/comm_convert.uvprojx。
- 默认DTO=0，HIK采集=1，运行时互斥，三路均可访问本机0x14；原proto_params/proto_dto已合并为proto_local。
- 采集：UART3/485口2轮询EVE/SZS；UART2/485口1响应海康；UART1为4G。
- 参数单页0x0801E000，显式56781保存、52445恢复默认，成功后重启；不恢复双页方案。
- 继电器仅两路，独立页0x0801F000自动保存逻辑请求；保护覆盖输出不改记忆，默认恢复不擦该页。
- 接线35按bit0/bit1配置NC/NO，GPIO＝保护后的逻辑值XOR接线位；不能固定假设逻辑值等于GPIO。
- 采集缓存无效/过期读0，不替代为ADC；剩余容量已按用户要求用标称容量×SOC/100估算。
- HIK SN固定16字节、左补0x00、仅RAM，切模式保留、实际重启清空；不加入参数保存。
- GM800配置23步、逐条应答、日志回发起485口；失败等待外部重发，不自动重试/重配。
- APP已链接0x08003800/0x1A800，VTOR已迁移；升级标志页0x08003000，升级0、正常55BB。
  BootLoad为严格8字节ASCII，无CRC/CRLF/ACK；保留真实链接范围及VTOR检查，不能移除保护。
- 固定鉴权后缀沿现有配置保留。用户已确认可公开推送，不重复询问，也不在说明中抄写值。

## 文档与验证入口

- [本机通信协议](Docs/本机通信协议_0x14.md)：寄存器、单项读写、CRC示例、保存与默认值。
- [采集与海康](Docs/采集与海康协议说明.md)：外设查询表、海康及RAM SN。
- [4G配置](Docs/4G模块配置说明.md)：23步、应答、超时、失败和日志。
- [引导接入](Docs/引导接入说明.md)：Flash布局、BootLoad、向量和升级边界。
- 原`.agents/tests`测试脚本已由用户删除；历史验证结果见交接记录，需要重跑时从Git历史恢复。
- 代码/构建配置变更后编译应用主工程，要求0 error、0 warning；仅文档和说明不必重编译。
- 编译前检查工程前后命令；现有AfterBuild为fromelf导出BIN，不等同烧录。
  结合进程结束、新日志与错误/警告数确认，不只看退出码或旧日志。
- 仅明确要求才烧录。`keil_build_flash.bash`无参数会编译并烧录，未获授权不要运行。
- 编译器路径、仅编译命令与历史测试说明见项目交接记录；工具不可用先查现有安装，不自行更换工具链。
- 软件回归、编译和用户上板反馈分开报告，不能把一种结果扩大为另一种验收。

## 自研C/H模板与格式

- 新文件使用[头文件/源文件模板技能](.agents/skills/c-header-template/SKILL.md)及assets。
- 修改自研C/H遵循[代码风格技能](.agents/skills/c-code-style/SKILL.md)和根.clang-format。
- 保留文件头/分区，四空格、Allman、80列目标、if(condition)；不顺带重排其他文件。
- 修改后用PowerShell 7运行`.agents/scripts/format-c-code.ps1`，显式列出本次自研C/H路径。
  只允许Bsp/Services/Protocols/App/Config，不传目录、通配符，不处理Core/Drivers。
- 新源码同步MDK分组与包含路径；模板资源不加入构建。阶段结束更新project-memory对应章节。
