# STM32F103C8T6 + AT7456E OSD 驱动项目

基于 STM32F103C8T6 通过 SPI 驱动 AT7456E OSD（屏幕叠加显示）芯片的嵌入式学习项目。

> 仅供学习与研究使用，禁止用于任何商业用途。
> Git 仓库：https://github.com/longwuhen321/Embedded_Project/tree/STM32F103C8T6_AT7456E

---

## 硬件平台

| 组件 | 型号 / 说明 |
|------|------------|
| MCU | STM32F103C8T6（72 MHz，HSE + PLL × 9） |
| OSD 芯片 | AT7456E |
| 开发框架 | STM32 HAL（STM32CubeMX 生成） |
| IDE | Keil MDK-ARM |

### 引脚连接

| 功能 | STM32 引脚 |
|------|-----------|
| AT7456E CS（片选） | PA4 |
| AT7456E SCK（时钟） | PA5 |
| AT7456E MISO | PA6 |
| AT7456E MOSI | PA7 |
| 状态 LED | PB12 |

### SPI 配置

- 模式：主机模式，全双工
- 数据位：8 bit
- 时钟极性：CPOL = HIGH，CPHA = 2EDGE（模式 3）
- 波特率预分频：32（约 2.25 MHz）
- 位序：MSB 优先
- NSS：软件控制

---

## 项目结构

```
Embedded_Project/
├── Core/
│   ├── Inc/
│   │   ├── AT7456E.h          # AT7456E 驱动头文件（寄存器定义、结构体、函数声明）
│   │   ├── AT7456E_font.h     # 字库数据头文件（由工具脚本生成）
│   │   ├── main.h             # 引脚宏定义
│   │   ├── spi.h / tim.h / gpio.h
│   │   └── stm32f1xx_*.h
│   └── Src/
│       ├── AT7456E.c          # AT7456E 驱动实现
│       ├── main.c             # 主程序入口
│       ├── spi.c              # SPI1 初始化
│       ├── tim.c              # TIM2 微秒延时
│       ├── gpio.c             # GPIO 初始化
│       └── stm32f1xx_*.c
├── Docs/
│   ├── Datasheet/
│   │   └── 芯片手册-AT7456E datasheet.pdf
│   └── Font/
│       ├── mcm_to_h.py        # MCM 字库转 C 头文件工具
│       ├── bfstyle_me.mcm     # 自定义字库文件
│       ├── DEFAULT.MCM        # 默认字库文件
│       ├── AT7456E_font.h     # 工具输出的字库头文件
│       └── AT7456(E)芯片配置&字库工具.exe
├── Drivers/                   # STM32 HAL + CMSIS 标准库
├── MDK-ARM/                   # Keil 工程文件
└── STM32F103C8T6_AT7456E.ioc  # STM32CubeMX 配置文件
```

---

## 功能说明

### 1. AT7456E 初始化

`AT7456E_Init()` 完成以下步骤：
1. 等待 500 ms 上电稳定
2. 软件复位芯片
3. 从 NVM 加载字库到 RAM
4. 自动识别视频制式（NTSC / PAL）
5. 配置 VM1 寄存器（背景模式、灰度级别、闪烁时间）
6. 禁用 OSD 黑电平自动调整
7. 清空屏幕显示

### 2. 视频制式支持

| 制式 | 行数 | 列数 |
|------|------|------|
| NTSC | 13 | 30 |
| PAL  | 16 | 30 |
| 自动 | 读 STAT 寄存器自动判断 | 30 |

### 3. 字符显示

提供两种寻址模式：

- **8 位模式**：支持 0~511 地址的字符，每个字符可独立设置属性（反色、闪烁、局部背景控制）
- **16 位模式**：支持 0~255 地址的字符，属性统一由 DMM 寄存器控制

两种模式均支持**自动递增（Auto-Increment）**批量写入。

主要接口：

```c
// 单字符显示
bool AT7456E_WriteChar(AT7456E_ShowChar_t show_char);

// 自动递增批量显示
bool AT7456E_WriteChar_AutoInc(AT7456E_ShowChar_AutoInc_t show_char_autoinc);
```

### 4. 字库操作

- **读取字库**：从 NVM 读取指定地址的字符点阵数据（54 字节/字符，12×18 像素，2bit/像素）
- **写入字库**：将新字符数据写入 NVM（支持普通模式和自动递增模式）
- **修改字库**：修改字库中某字符的单字节或连续多字节
- **校验字库**：对比 NVM 中的字库与内置字库数组，不一致时自动更新

### 5. 字库转换工具

`Docs/Font/mcm_to_h.py` 将 MCM 格式的字库文件转换为 C 头文件：

```bash
python mcm_to_h.py
# 输入：bfstyle_me.mcm
# 输出：build/AT7456E_font.h
```

MCM 文件可通过 `AT7456(E)芯片配置&字库工具.exe` 进行编辑和自定义。

---

## 快速上手

1. 使用 STM32CubeMX 打开 `STM32F103C8T6_AT7456E.ioc` 核对配置（无需重新生成）
2. 用 Keil MDK 打开 `MDK-ARM/` 目录下的工程文件
3. 编译并烧录到 STM32F103C8T6 开发板
4. 运行后主循环调用 `AT7456E_TEST()` 演示字符显示与字库修改功能

---

## 参考资料

- AT7456E 数据手册：`Docs/Datasheet/芯片手册-AT7456E datasheet.pdf`
- AT7456(E) 演示程序说明：`Docs/Font/AT7456(E)演示程序说明.pdf`
- STM32F1xx HAL 库文档：`Drivers/STM32F1xx_HAL_Driver/`

---

## License

本项目仅供学习与研究，转载请保留出处，禁止用于商业用途。
