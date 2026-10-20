# AGENTS.md

## 工作规则
- 开发前阅读 `.agents/project-memory.md`，核对已确认需求、源码事实及待办。
- Windows 命令始终使用 PowerShell 7：
  `C:\Program Files\PowerShell\7\pwsh.exe`，不使用 `powershell.exe`。
- 保留用户改动，只修改当前需求涉及的文件；讨论和计划不能写成已实现结果。
- 当前Git根目录Application是应用主工程；旧Program/app-demo仅为迁移历史，
  不随当前仓库保存、不加入构建。回归样例已固定，不依赖旧参考目录。
  同级Bootload由用户维护，未明确要求时不检查或修改。
- `Core/` 保留 CubeMX 初始化及中断入口，优先在 USER CODE 区接入自研接口；
  不把业务写入生成区，不批量格式化生成代码、厂商库或参考工程。
- `Drivers/` 是厂商 HAL/CMSIS，原则上保持原样；必要修改须说明原因与影响。
- 裸机、静态分配，不使用 malloc/free 或引入 RTOS。
- 校验指针、长度、容量、索引和等待超时；共享数据明确所有权与同步方式。
  临界区保持短小，保存、恢复原中断状态，不能无条件开中断。
- 中断只完成收发、标记及必要数据搬运，不执行阻塞等待、延时、格式化日志
  或协议业务。RS485 发送完成后才能结束本次发送并恢复接收。
- 不猜测引脚极性、继电器状态、采样比例、协议地址、波特率或 Flash 布局。
  不直接搬入旧云端凭据、设备身份或未经确认的默认参数。

## 工程事实
- MCU：STM32G070KBT6 / Cortex-M0+，裸机 HAL。
- 主工程：`MDK-ARM/comm_convert.uvprojx`；CubeMX：`comm_convert.ioc`。
- 当前工程配置：ARM Compiler 5.06 update 6、Keil.STM32G0xx_DFP.2.1.0，
  STM32Cube FW_G0 V1.6.3。以当前文件为准，不沿用参考工程版本。
- 芯片 Flash 128 KiB / SRAM 36 KiB；MDK应用起始地址0x08003800，长度
  0x1A800（106 KiB），0x0801E000以上保留。参数只用0x0801E000单页，
  原备份地址 0x0801E800 不访问、不擦写，可另行规划。用户已授权继电器
  状态沿用原页 0x0801F000，只由继电器维护流程写入，不由参数保存/恢复默认擦写。
- SYSCLK 为 64 MHz，AHB 分频后 HCLK/CPU 为 16 MHz；不能混淆两者。
- UART1：4G 模块，115200 8N1；UART2/3：RS485，9600 8N1，硬件 DE 高有效。
- ADC1：PA0，12 位、连续转换，无 ADC DMA；TIM14/TIM16、IWDG 已生成初始化。
  初始化存在不代表业务已接入或外设已启动。

## 引导入口（APP地址尚待用户调整）
- 旧ASCII BootLoad严格8字节（42 6F 6F 74 4C 6F 61 64），无CRC/CRLF/ACK；
  两模式三口由app_local统一识别，TX排空、RX暂停后写升级标志0并校验再系统复位。
- boot_config.h定义标志页0x08003000、APP起点0x08003800、上限0x0801E000，
  正常标志55BB。仅真实装载基址/范围及VTOR均匹配时允许写。用户已将APP链接改为0x08003800/0x1A800，main USER CODE
  已接VTOR及恢复引导关闭的全局中断。保留实际布局检查，不得移除保护凑测试。
- 安全布局启动由svc_boot维护55BB；失败记数，不自动反复擦写。升级写失败恢复RX，
  不自动重试/复位。引导页不由参数/继电器存储维护。Boot端擦除范围待用户核对。
- 用户自行调整APP链接；代理按后续授权仅在main USER区补头文件和恢复中断，
  未改scatter或Bootloader。详见.agents/boot-bringup.md。

## 目标模式与分层
- 采集模式：UART3（485口2）主动轮询，整理并缓存数据；UART2（485口1）响应海康主机查询；
  UART1 用于 4G 配置，485 配置入口需支持重复配置。
- DTO 模式：UART1 业务数据同时投递 UART2/3；两路回复均返回 UART1。
  两路独立缓冲，UART1 串行发送，禁止回复交叉覆盖。
- 共用底层、运行时互斥；用户已选默认DTO（0）、采集HIK（1）。当前支持
  UART1和两路485均可访问完整本机协议0x14，寄存器34读写模式；ACK和三路TX排空后，清旧协议缓存并等
  700ms总线静默再切换，无重启/自动Flash写。模式变化仅RAM，显式56781
  三路均可显式保存到原参数单页后重启。协议已统一为Protocols/proto_local.c/.h，
  原proto_params/proto_dto已移除，17～43可跨区连读，见 `.agents/local-protocol.md`。
  有保存的模式优先，否则含旧参数记录均默认DTO。
  切换只清通信/采集缓存，不清公共继电器/保护请求/参数/SN。20s无法完成时
  取消目标并计失败，待发送结束及静默后恢复原模式。见 `.agents/mode-bringup.md`。
- 继电器、掉电记忆、欠压保护和参数服务为两模式共用功能；统一复用
  Services/svc_relay，协议仅解码并调用公共接口，不建立各自的继电器状态。
  后续切换模式不因切换而重新初始化继电器或清除保护/记忆请求。
- 当前默认启用 COMM_APP_ENABLE_MODE=1，DTO/HIK两套实现同时编译，由
  App/app_mode独占调度，每轮只运行当前模式，不允许两个RX消费者同时执行。
  UART2 处理 DD/A5 的 03/0F/0C 读请求；UART3 已接 EVE/SZS 轮询，默认先 EVE，
  连续 5 s 无有效数据回复切换尝试另一套；查询表和换算来自 app-demo。
  proto_controller 仅编解码，svc_poll 单事务调度并发布 svc_telemetry；app_hik
  仍独占 RX，先识别配置再分发回复，禁止 Services 再消费同一端口队列。
  1 s 查询间隔沿用旧调度；600 ms 响应/失败静默窗口为本轮待上板参数。
  配置/Flash 维护期间暂停轮询并使采集缓存失效，结束后重新读固定信息；
  不清除公共继电器、参数、SN。用户已确认EVE/SZS均上板可用，见 `.agents/poll-bringup.md`。
  用户已明确无有效外部缓存时 03/0F 也正常回复，测量值填零；缓存有效标记
  保持无效，不把回包等同采集成功。0C 沿用旧固定信息查询。
  用户已要求接入 SN 仅做 RAM 缓存：HIK 5A/A3 写、A5/0D 读，写入固定 16 字节，
  左侧补 0x00（不是 ASCII 0x30），原样保留；确认帧为 DD A3 00 00 00 00 77。
  svc_identity 启动清空，不读写 Flash、不随 56781 保存，不在切换协议时清空。
  两个分支不得同时消费RX。单模式专测需显式 COMM_APP_ENABLE_MODE=0，
  再选择DTO=1/HIK=0或DTO=0/HIK=1；生产构建使用运行时调度。
  缓存/采样组有效期7s，避免EVE完整5s周期的边缘误失效，5s无回复换型不变。
- GM800配置由两路485触发，两模式共用，保留21条旧指令，
  CLOUD后、保存前增加AT+ICCID?和AT+IMEI?（CRLF结尾），共23步；至少200ms间隔。
  已接逐条应答：+++等a、a等+ok、普通AT等完整OK行；普通超时2s，保存5s，
  会话上限90s。ERROR/超时/传输异常立即停后续步骤，收尾排空日志后退出，
  不自动重试/重配，不因配置失败调用复位；只等外部重新发送配置指令。
  成功仍保留两次AT+S及AT+Z，CONFIRMED只表示23条应答通过，不证明平台上线。
  原始回复及完成/失败步骤提示回发起485口，日志满不影响协议确认。
  配置期间暂停DTO/采集，保存等业务指令丢弃；结束收尾500ms并排空TX。
  用户已确认旧流程日志全部OK、模块重启后平台上线；新应答门控已验证DTO两口及HIK485口1，HIK485口2未明确实测。
  旧固定鉴权后缀按已确认流程保留，不在记忆或输出中重复密钥。
  当前范围和验证见 `.agents/gm800-bringup.md`。
- 用户已分别验证两个 485 口均可配置 4G、平台上线。按用户要求参数采用
  单页，低频/出厂配置为主，不恢复双页轮换。已接三路本机参数段26～33；启动时
  有效旧参数可覆盖 485 波特率，范围见 `.agents/params-bringup.md`。
- 本机地址0x14从UART1/2/3拦截，统一app_local入口并原口回复，保存/恢复默认口令沿用旧版，先确认回包和
  三路 TX 完成，再暂停 RX、单页写入、校验成功后重启；失败不反复擦写。
  已接两路继电器 17/18 控制，19/20 仅反向只读别名，3/4 硬件移除。
  默认接线配置35=0时，无有效记忆保持用户初始化低电平（NC闭合、NO断开）。
  35按bit0/bit1分别配置继电器1/2：0常闭保持原GPIO，1常开取反，合法值0～3。
  公共svc_relay统一GPIO=保护覆盖后的逻辑输出 XOR 对应接线位，手动/保护/恢复
  均经此映射；17/18及19/20读逻辑状态，Watch的g_relay.gpio为物理电平。
  35仅RAM生效，56781才保存；参数记录第二个64位字bits48..55使用预留字节存mask，
  bits56..63仍0，24字节/单页/校验不变，旧记录mask=0。继电器页仍只存逻辑请求。
  36～43依次只读电池电压、电流、SOC、剩余/标称容量、温度1、太阳能电压/电流；
  使用外部有效缓存，过期/未采集回0，不用ADC替代；电流/温度有符号。
  用户已要求剩余容量按标称容量×SOC/100估算，svc_poll发布缓存前用32位乘法
  统一计算，保持0.01Ah单位并向下截断；容量缺失为0时估算也为0。
  继电器回包完成后自动保存，不重启；保护覆盖输出但不改写记忆请求。
  DTO 本机 0x14 的 SN 未实现，HIK SN 仅 RAM；升级不伪造成功。
  测试见 `.agents/dto-relay-bringup.md` 和 `.agents/hik-bringup.md`。

| 目录 | 职责 |
| --- | --- |
| `Core/` | CubeMX 初始化、中断入口、最小装配 |
| `Bsp/` | UART、ADC、定时器、GPIO 等硬件操作，封装 HAL |
| `Services/` | 收发缓冲、参数存储、采样、周期调度、4G 配置流程 |
| `Protocols/` | 协议解析、校验、组包；不直接驱动外设 |
| `App/` | 采集、DTO、模式管理及业务装配 |
| `Config/` | 引脚用途、默认参数、功能开关及配置类型 |

- App 使用 Services 和 Protocols；Services 使用 Bsp，必要时调用协议编解码。
  下层不得反向依赖 App；协议模块保持独立，参数和缓冲区显式传入。
- HAL 类型仅在 Core、Bsp 和硬件绑定配置内使用，避免进入业务公开接口。
- 每类 HAL 回调只保留一个实现，由底层统一分发，避免同名回调冲突。
- 六层是目标结构；未创建的目录不表示已经迁移，不为凑目录添加空函数。

## 验证
- 代码或构建配置修改后编译根目录主工程，目标为 0 error、0 warning。
- 已确认本机 Keil 存在：
  `D:\software_config\keil_soft\keil_app\UV4\UV4.exe`。
  仅构建示例（工程根目录、PowerShell 7）：
  ```powershell
  & 'D:\software_config\keil_soft\keil_app\UV4\UV4.exe' `
      -b '.\MDK-ARM\comm_convert.uvprojx' -t 'comm_convert' `
      -j0 -o '.\MDK-ARM\build-check.log'
  ```
- 执行前检查构建前后命令，避免附带烧录；结合进程完成状态、新日志及
  错误/警告数判断，不以旧日志或退出码单独宣称成功。
- 仅用户明确要求时烧录，不使用 `-f` 或自动下载脚本。
- 工具不可用时先检查现有安装并报告限制，不擅自更换编译器或工具链。
- 纯文档、技能说明及未接入构建的模板无需编译；脚本变更验证实际行为。
- 软件检查、编译与上板验证分别报告，不能相互替代。

## 自研 C/H 模板与格式
- 新建自研 C/H 使用 `.agents/skills/c-header-template/SKILL.md` 及 assets；
  编写或修改自研 C/H 使用 `.agents/skills/c-code-style/SKILL.md`。
- 保留模板文件头和分区，四空格缩进；根目录 `.clang-format` 为排版来源，
  重点遵循 80 列目标、Allman 大括号及 `if(condition)` 格式。
- 修改后用 PowerShell 7 调用 `.agents/scripts/format-c-code.ps1`，显式列出
  本次修改的自研 C/H 路径；仅允许 Bsp、Services、Protocols、App、Config。
  不传目录、通配符，不批量重排生成代码或参考工程。
- 模板资源不加入 MDK；新源码同步加入主工程分组与包含路径。
- 阶段完成后更新 `.agents/project-memory.md`；开发顺序与验收见
  `.agents/development-plan.md`。
