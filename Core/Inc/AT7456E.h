/**
 * @file AT7456E.h
 * @author 龙无痕 (1365149109@qq.com)
 * @brief AT7456E OSD驱动头文件
 * @version 1.0
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026
 * 
 * @note 本代码仅供学习与研究使用。
 * @note 转载请保留出处，禁止用于商业用途。
 * @note git仓库地址：https://github.com/longwuhen321/Embedded_Project/tree/STM32F103C8T6_AT7456E
 */
#define __AT7456E_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "spi.h"
#include "main.h"

#define AT7456E_CS_LOW()   HAL_GPIO_WritePin(AT7456E_CS_GPIO_Port, AT7456E_CS_Pin, GPIO_PIN_RESET)
#define AT7456E_CS_HIGH()  HAL_GPIO_WritePin(AT7456E_CS_GPIO_Port, AT7456E_CS_Pin, GPIO_PIN_SET)
    
// 参考手册第18页 SPI接口说明中的图15 图16
#define DIR_READ(a) ((a) | (1 << 7))    // 读操作设置A7=1
#define DIR_WRITE(a) ((a) & 0x7f)       // 写操作设置A7=0

#define OSD_CHARS_PER_ROW	30  // 每行字符数目

// 寄存器地址定义 读时需要与上 0x80;
#define AT7456E_VM0             0X00
#define AT7456E_VM1             0X01
#define AT7456E_HOS             0X02
#define AT7456E_VOS             0X03
#define AT7456E_DMM             0X04
#define AT7456E_DMAH            0X05
#define AT7456E_DMAL            0X06
#define AT7456E_DMDI            0X07
#define AT7456E_CMM             0X08
#define AT7456E_CMAH            0X09
#define AT7456E_CMAL            0X0A
#define AT7456E_CMDI            0X0B
#define AT7456E_OSDM            0X0C
#define AT7456E_OSDBL           0X6C
#define AT7456E_STAT            0XA0
#define	AT7456E_DMDO            0x30	// 显示内存数据输出
#define	AT7456E_CMDO            0x40	// 字符内存数据输出

#define AT7456E_NVM_RAM         0x50    // 将NVM中的字库读取到镜像RAM中
#define AT7456E_RAM_NVM         0xA0    // 将镜像RAM中的字库数据写到NVM中

// DMM寄存器各位定义
#define DMM_BIT_AUTO_INC_CHAR       ((uint8_t)(1 << 7))  // 位7：字符存储器自动递增模式
#define DMM_BIT_OP_MODE             ((uint8_t)(1 << 6))  // 位6：操作模式选择(设置为1时为8位,0为16位)
#define DMM_BIT_ATTR_LBC            ((uint8_t)(1 << 5))  // 位5：本地背景控制(LBC)
#define DMM_BIT_ATTR_BLK            ((uint8_t)(1 << 4))  // 位4：闪烁(BLK)
#define DMM_BIT_ATTR_INV            ((uint8_t)(1 << 3))  // 位3：反色(INV)
#define DMM_BIT_CLEAR_MEM           ((uint8_t)(1 << 2))  // 位2：清除显示存储器
#define DMM_BIT_VSYNC_CLEAR         ((uint8_t)(1 << 1))  // 位1：垂直同步清除
#define DMM_BIT_AUTO_INC_DISPLAY    ((uint8_t)(1 << 0))  // 位0：显示存储器自动递增模式
#define DMM_NONE                    ((uint8_t)0x00)      // 默认属性

// 单字符显示属性位定义
#define DMDI_ATTR_LBC               ((uint8_t)(1 << 7))  // 位7: 本地背景控制(LBC)
#define DMDI_ATTR_BLK               ((uint8_t)(1 << 6))  // 位6: 闪烁(BLK)
#define DMDI_ATTR_INV               ((uint8_t)(1 << 5))  // 位5: 反色(INV)
#define DMDI_ATTR_CA8               ((uint8_t)(1 << 4))  // 位4: 字符地址第8位
#define DMDI_ATTR_NONE              ((uint8_t)0x00)      // 默认属性

// 视频制式枚举
typedef enum {
    VIDEO_STD_UNKNOWN   = 0,    // 未知制式
    VIDEO_STD_PAL       = 1,    // NTSC制式
    VIDEO_STD_NTSC      = 2     // PAL制式    
} Video_Standard;

// OSD显示状态枚举
typedef enum {
    OSD_STATE_OFF = 0,  // 关闭OSD显示
    OSD_STATE_ON  = 1   // 开启OSD显示
} AT7456E_OSD_State;

/***************** VM1寄存器位定义（视频模式寄存器1）*****************/
// 位7：背景模式
#define VM1_BACKGROUND_MODE_LOCAL    0x00    // 0：本地背景控制（由DMM[5]和DMDI[7]控制）
#define VM1_BACKGROUND_MODE_GRAY     0x80    // 1：所有背景像素设为灰色

// 位[6:4]：背景亮度（灰色电平百分比）
#define VM1_GRAY_LEVEL_0      (0x00 << 4)    // 000 = 0%
#define VM1_GRAY_LEVEL_7      (0x01 << 4)    // 001 = 7%
#define VM1_GRAY_LEVEL_14     (0x02 << 4)    // 010 = 14%
#define VM1_GRAY_LEVEL_21     (0x03 << 4)    // 011 = 21%
#define VM1_GRAY_LEVEL_28     (0x04 << 4)    // 100 = 28%（默认值）
#define VM1_GRAY_LEVEL_35     (0x05 << 4)    // 101 = 35%
#define VM1_GRAY_LEVEL_42     (0x06 << 4)    // 110 = 42%
#define VM1_GRAY_LEVEL_49     (0x07 << 4)    // 111 = 49%

// 位[3:2]：闪烁时间
#define VM1_BLINK_TIME_2FIELDS   (0x00 << 2)  // 00 = 2场（NTSC:33ms, PAL:40ms）
#define VM1_BLINK_TIME_4FIELDS   (0x01 << 2)  // 01 = 4场（NTSC:67ms, PAL:80ms）（默认）
#define VM1_BLINK_TIME_6FIELDS   (0x02 << 2)  // 10 = 6场（NTSC:100ms, PAL:120ms）
#define VM1_BLINK_TIME_8FIELDS   (0x03 << 2)  // 11 = 8场（NTSC:133ms, PAL:160ms）

// 位[1:0]：闪烁占空比（亮:暗）
#define VM1_BLINK_DUTY_1_1      (0x00 << 0)  // 00 = BT : BT
#define VM1_BLINK_DUTY_1_2      (0x01 << 0)  // 01 = BT : (2×BT)
#define VM1_BLINK_DUTY_1_3      (0x02 << 0)  // 10 = BT : (3×BT)
#define VM1_BLINK_DUTY_3_1      (0x03 << 0)  // 11 = (3×BT) : BT（默认）



/**************** 显示字符相关 ****************/
typedef enum {
    Show_Char_16bit = 0,    // 16位模式
    Show_Char_8bit          // 8位模式
} AT7456E_ShowChar_Mode;

// 显示字符结构(非自动递增模式)
typedef struct {
    AT7456E_ShowChar_Mode mode;     // 字符显示模式 (8位模式或16位模式)
    uint16_t    addr;               // 字符显示地址 (8位模式：0-511; 16位模式：0-255)
    uint8_t     attr;               // 字符显示属性 (8位模式：配置DMDI寄存器实现单字符属性设置，16位模式：配置DMM寄存器统一设置属性)
    uint8_t     row;                // 字符显示行号 (0-15)
    uint8_t     col;                // 字符显示列号 (0-29)
} AT7456E_ShowChar_t;

// 显示字符结构(自动递增模式)
typedef struct {
    AT7456E_ShowChar_Mode mode;     // 显示字符模式 (8位模式或16位模式)
    uint8_t    *addr_array;        // 字符显示地址数组指针 (0-255)
    uint8_t     length;             // 字符地址数组长度
    uint8_t     attr;               // 字符显示属性 (8位模式可单字符设置，16位模式统一设置DMM寄存器)
    uint8_t     row;                // 显示行号 (0-15)
    uint8_t     col;                // 显示列号 (0-29)
} AT7456E_ShowChar_AutoInc_t;


/**************** 字库相关 ****************/
// 字库写入模式枚举
typedef enum {
    AT7456E_WRITE_FONT_NORMAL = 0,   // 普通写入模式
    AT7456E_WRITE_FONT_AUTO_INC = 1  // 自动递增写入模式
} AT7456E_WriteFont_Mode;

#define AT7456E_FONT_SIZE 54  // 每个字符字体数据大小，单位：字节

// 字库字符数据结构
typedef struct {
    uint16_t addr;           // 字符地址 (0-511)
    
    union{
        uint8_t *data_array;                // 字符字体数据数组指针 (54字节)
        const uint8_t *const_data_array;    // const类型字符字体数据数组指针 (54字节)
    }data;
} AT7456E_CharFont_t;

/**************** SPI通信相关函数 ****************/
void AT7456E_SPI_Send(const uint8_t *data, uint16_t len);
void AT7456E_SPI_SendByte(uint8_t data);
uint8_t AT7456E_Read_Reg(uint8_t addr);
void AT7456E_Write_Reg(uint8_t addr, uint8_t data);

/**************** AT7456E初始化相关函数 ****************/
void AT7456E_Init(void);
void AT7456E_VM1_Init(void);


/**************** 显示字符相关函数实现 ****************/
bool AT7456E_WriteChar(AT7456E_ShowChar_t show_char);
bool AT7456E_WriteChar_8bit(AT7456E_ShowChar_t show_char);
bool AT7456E_WriteChar_16bit(AT7456E_ShowChar_t show_char);
bool AT7456E_WriteChar_AutoInc(AT7456E_ShowChar_AutoInc_t show_char_autoinc);
bool AT7456E_WriteChar_AutoInc_8bit(AT7456E_ShowChar_AutoInc_t show_char_autoinc);
bool AT7456E_WriteChar_AutoInc_16bit(AT7456E_ShowChar_AutoInc_t show_char_autoinc);

/**************** 字库相关函数实现 ****************/
bool AT7456E_Check_Font(void);
bool AT7456E_ReadChar_Font(AT7456E_CharFont_t *font);
bool AT7456E_WriteChar_Font(const AT7456E_CharFont_t *write_font, AT7456E_WriteFont_Mode write_font_mode);
bool AT7456E_ModifyChar_Byte_Font(uint16_t char_addr, uint8_t byte_pos, uint8_t new_byte);
bool AT7456E_ModifyChar_ByteArray_Font(uint16_t char_addr, uint8_t byte_pos, const uint8_t *modify_array, uint8_t len);

/**************** 工具相关函数实现 ****************/
void AT7456E_ClearSRAM(void);
void AT7456E_Clear_Show(void);
void AT7456E_Set_OSD_State(AT7456E_OSD_State state);
bool AT7456E_NVM_Operation(uint8_t operation);
void AT7456E_Show_Full_Icon(bool hight_addr);

/**************** 测试相关函数实现 ****************/
void AT7456E_TEST(void);
void AT7456E_WriteChar_Test(bool status);
void AT7456E_WriteChar_AutoInc_Test(void);
void AT7456E_ModifyChar_Byte_TEST(void);






#ifdef __cplusplus
}
#endif

#endif /* __AT7456E_H */
