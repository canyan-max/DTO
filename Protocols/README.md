# 协议层

proto_local.c/.h 是唯一的本机0x14协议：继电器、版本、参数、保存/恢复默认、模式、继电器接线及只读采集数据。
寄存器17～43连续读取，保留03/旧01/05/06/10的已支持功能。UART1/2/3在DTO和
采集模式下都可访问完整本机协议，App/app_local应用命令并从原端口回复。
原proto_params、proto_dto四个文件及构建项已移除。寄存器说明见
[统一本机协议](../Docs/本机通信协议_0x14.md)。

其他模块保持各自外部协议职责：
- proto_gm800：AT+CLOUD请求解析、流式应答识别及23步4G配置指令组包。
- proto_hik：海康DD/A5读数据、RAM SN写入/读回及整数数据组包。
- proto_xdz：XDZ地址05双组轮询编解码、地址02/04读取区及10/0425双继电器控制建议。
- proto_controller：旧EVE/SZS查询表、长度/地址/CRC校验及整数换算。

协议仅解析/校验/组包，使用显式输入快照并输出命令建议；不访问HAL/UART句柄、
不定义回调、不驱动GPIO/Flash、不决定端口所有权。app_mode互斥调度DTO/HIK/XDZ，
svc_params、svc_relay、svc_identity继续共用；SN仍只在RAM，不纳入56781保存。
