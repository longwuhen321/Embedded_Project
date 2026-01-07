/**
 * @file AT7456E.c
 * @author 龙无痕 (1365149109@qq.com)
 * @brief AT7456E OSD驱动源文件
 * @version 1.0
 * @date 2026-01-07
 * 
 * @copyright Copyright (c) 2026
 * 
 * @note 本代码仅供学习与研究使用。
 * @note 转载请保留出处，禁止用于商业用途。
 * @note git仓库地址：https://github.com/longwuhen321/Embedded_Project/tree/STM32F103C8T6_AT7456E
 */
#include "main.h"
#include "AT7456E.h"
#include "AT7456E_font.h"

/**************** SPI通信 ****************/
/**
 * @brief 向AT7456E发送SPI数据
 * 
 * @param data 要发送的数据指针
 * @param len 要发送的数据长度
 */
void AT7456E_SPI_Send(const uint8_t *data, uint16_t len)
{
    AT7456E_CS_LOW();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, HAL_MAX_DELAY);
    AT7456E_CS_HIGH();
}

// 发送单个字节的函数
void AT7456E_SPI_SendByte(uint8_t data)
{
    AT7456E_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    AT7456E_CS_HIGH();
}

/**
 * @brief 向AT7456E写入寄存器
 * 
 * @param addr 寄存器地址
 * @param data 要写入的数据
 */
void AT7456E_Write_Reg(uint8_t addr, uint8_t data)
{
    uint8_t buf[2];

    buf[0] = DIR_WRITE(addr);   // 写：A7 = 0
    buf[1] = data;

    AT7456E_CS_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    AT7456E_CS_HIGH();
}

/**
 * @brief 从AT7456E读取寄存器
 * 
 * @param addr 寄存器地址
 * @return 读取到的数据
 */
uint8_t AT7456E_Read_Reg(uint8_t addr)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = DIR_READ(addr); // 读：A7 = 1
    tx[1] = 0xFF;           // 置换字节

    AT7456E_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    AT7456E_CS_HIGH();

    return rx[1];
}

/**************** AT7456E初始化相关函数 ****************/

/**
 * @brief AT7456E初始化
 */
void AT7456E_Init(void)
{    
    // 初始化操作之前必须延时50ms以上
    // 手册中35页 “各操作命令的实际执行时间对照表” 有描述   
    HAL_Delay(500); 
   
    // 软件复位 VM0[1]=1
    AT7456E_Write_Reg(AT7456E_VM0,0x02);
    while((AT7456E_Read_Reg(AT7456E_VM0) & 0x04));// 等待清除完成 DMM[2]=0    

    // 检查是否需要修改字库
    // AT7456E_Check_Font();    
    
    // 从NVM读取到镜像RAM
    AT7456E_NVM_Operation(AT7456E_NVM_RAM);

    // 读取状态寄存器，判断视频制式
    uint8_t read_status;
    read_status = AT7456E_Read_Reg(AT7456E_STAT);
    Video_Standard video_standard = (Video_Standard)(read_status & 0x03);
    switch (video_standard)
    {
    case VIDEO_STD_PAL:
        AT7456E_Write_Reg(AT7456E_VM0,0x48);
        break;
    
    case VIDEO_STD_NTSC:
        AT7456E_Write_Reg(AT7456E_VM0,0x08);
        break;
    
    default:
        AT7456E_Write_Reg(AT7456E_VM0,0x48);
        break;
    }

    HAL_Delay(1);  

    // 配置VM1寄存器
    AT7456E_VM1_Init();

    AT7456E_Write_Reg(AT7456E_OSDBL,(AT7456E_Read_Reg(AT7456E_OSDBL)|0X10));    // OSD黑电平禁止自动控制

    AT7456E_Write_Reg(AT7456E_OSDM,0x2D);
    HAL_Delay(1);
    
    // 清屏
//    AT7456E_ClearSRAM();  
    AT7456E_Clear_Show();

}

/**
 * @brief 配置VM1寄存器
 */
void AT7456E_VM1_Init(void)
{
    uint8_t vm1_config = VM1_BACKGROUND_MODE_LOCAL |    // 位7=0
                        VM1_GRAY_LEVEL_28 |             // 位[6:4]=100
                        VM1_BLINK_TIME_4FIELDS |        // 位[3:2]=01
                        VM1_BLINK_DUTY_1_1;             // 位[1:0]=00
    AT7456E_Write_Reg(AT7456E_VM1,vm1_config);
    HAL_Delay(1);      



}

/**************** 显示字符相关函数实现 ****************/

/**
 * @brief 在屏幕指定位置显示字符(最多16 * 30 = 480 个位置)
 * @param show_char 显示字符结构(非自动递增模式)
 */
bool AT7456E_WriteChar(AT7456E_ShowChar_t show_char)
{
    if (show_char.mode == Show_Char_8bit) {
        return AT7456E_WriteChar_8bit(show_char);
    } else if (show_char.mode == Show_Char_16bit) {
        return AT7456E_WriteChar_16bit(show_char);
    }

    return true;
}

/**
 * @brief 8位模式下在屏幕指定位置显示字符(最多16 * 30 = 480 个位置),可单独设置字符属性
 * @brief 参考手册34页 "8位模式下，写入显示存储器的步骤"
 * @param show_char 显示字符结构体
 * @param show_char.col 列号(0-29) 
 * @param show_char.row 行号(0-15)
 * @param show_char.addr 字符地址(0-511)
 * @param show_char.attr 字符属性 (位4: CA[8], 位5: 反色, 位6: 闪烁, 位7: 本地背景控制)
 */
bool AT7456E_WriteChar_8bit(AT7456E_ShowChar_t show_char)
{
    // 参数检查
    if (show_char.col >= OSD_CHARS_PER_ROW || show_char.row > 15 || show_char.addr > 511)
    {
        return false;
    }
    
    // 计算屏幕位置
    uint16_t screen_pos = show_char.row * 30 + show_char.col;

    // 准备属性字节
    uint8_t char_attr = 0x00;

    // 确保在8位模式
    AT7456E_Write_Reg(AT7456E_DMM, DMM_BIT_OP_MODE);  // DMM[6]=1

    // 写入显示的位置
    AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) | 0x02);// DMAH[1] = 1,DMAH[0]＝x
    AT7456E_Write_Reg(AT7456E_DMAL, screen_pos & 0xFF);

    // 设置DMDI[4] 既字符地址中的 CA[8]
    if(show_char.addr >= 256) {
        char_attr |= DMDI_ATTR_CA8;  // DMDI[4]=1 (CA[8]=1)
        
    }

    // 设置其他属性位
    // 注意：写显示字符属性不能按照手册 34 页中一样的顺序来写 否则不能正常显示储存地址为256至511的字符
    char_attr |= show_char.attr;

    // 写入字符显示属性字节
    AT7456E_Write_Reg(AT7456E_DMDI, char_attr);

    // 设置写入内容为字符地址 DMAH[1] = 0
    AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) & 0x01);// DMAH[1] = 0,DMAH[0]＝x

    // 显示存储器的字符地址字节（CA[7:0]）写入到DMDI[7:0]中
    AT7456E_Write_Reg(AT7456E_DMDI, (uint8_t)(show_char.addr & 0xFF));

    return true;
}

/**
 * @brief 16位模式下在屏幕指定位置显示字符(最多16 * 30 = 480 个位置),不可以单独设置字符属性
 * @brief 参考手册34页 "在16位模式下，写入显示存储器的步骤"
 * @param show_char 显示字符结构体
 * @param show_char.col 列号(0-29) 
 * @param show_char.row 行号(0-15)
 * @param show_char.addr 字符地址(0-255)
 * @param show_char.attr 配置DMM寄存器的属性位(不包含DMM[6]，该位在16位模式下必须为1)
 * @param show_char.attr 位5：本地背景控制(LBC) 位4：闪烁(BLK) 位3：反色(INV)
 * @return 成功返回true
 */
bool AT7456E_WriteChar_16bit(AT7456E_ShowChar_t show_char)
{
    if (show_char.addr > 255 || show_char.col >= OSD_CHARS_PER_ROW || show_char.row >= 16) {
        return false;
    }

    // 读取当前DMM寄存器值，避免重复写入
    if (show_char.attr  != AT7456E_Read_Reg(AT7456E_DMM)) {
        AT7456E_Write_Reg(AT7456E_DMM, show_char.attr);  // 写入DMM[6]＝0，选择16位工作模式
    } 

    // 计算显示位置
    uint16_t screen_pos = show_char.row * 30 + show_char.col;

    // 写入显示的地址
	AT7456E_Write_Reg(AT7456E_DMAH, screen_pos >> 8);
	AT7456E_Write_Reg(AT7456E_DMAL, screen_pos & 0xFF);

    // 写入显示存储器的字符地址字节（CA[7:0]）
	AT7456E_Write_Reg(AT7456E_DMDI, show_char.addr & 0xFF);

    return true;
}

/**
 * @brief 在屏幕指定位置显示字符(最多16 * 30 = 480 个位置)
 * @param show_char_autoinc 显示字符结构(自动递增模式)
 */
bool AT7456E_WriteChar_AutoInc(AT7456E_ShowChar_AutoInc_t show_char_autoinc)
{
    if (show_char_autoinc.mode == Show_Char_8bit) {
        return AT7456E_WriteChar_AutoInc_8bit(show_char_autoinc);
    } else if (show_char_autoinc.mode == Show_Char_16bit) {
        return AT7456E_WriteChar_AutoInc_16bit(show_char_autoinc);
    }

    return true;
}

/**
 * @brief 自动递增模式中的：8位模式下在屏幕指定位置显示字符
 * @brief 参考手册34页 "在自动递增模式中，写入显示存储器的步骤"
 * @param show_char 显示字符结构体
 * @param show_char.col 起始列号(0-29)
 * @param show_char.row 起始行号(0-15)
 * @param show_char.addr 字符地址(0-254 0xFF需要用来做停止标识符)
 * @param show_char.attr 字符属性 位5: 反色, 位6: 闪烁, 位7: 本地背景控制 (注意：不支持设置位4: CA[8])
 */
bool AT7456E_WriteChar_AutoInc_8bit(AT7456E_ShowChar_AutoInc_t show_char_autoinc)
{
    // 参数检查
    if (show_char_autoinc.col >= OSD_CHARS_PER_ROW || show_char_autoinc.row > 15)
    {
        return false;
    }
    
    // 计算屏幕位置
    uint16_t screen_pos = show_char_autoinc.row * 30 + show_char_autoinc.col; 

    // 检查是否超过显示区域
    if (screen_pos + show_char_autoinc.length > 479) {
        return false;

    }

    // 写入显示的位置
    AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) & 0x01);// DMAH[1] = 0,DMAH[0]＝x
    AT7456E_Write_Reg(AT7456E_DMAL, screen_pos & 0xFF);   

    // 确保在8位模式并且开启自动递增模式 DMM[0]=1, DMM[6]=1
    AT7456E_Write_Reg(AT7456E_DMM, AT7456E_Read_Reg(AT7456E_DMAH) | DMM_BIT_AUTO_INC_DISPLAY | DMM_BIT_OP_MODE);     

    for (uint8_t i = 0; i < show_char_autoinc.length; i++) {   
        // 检查发送的字符地址是否为 0xFF
        if (show_char_autoinc.addr_array[i] != 0xFF){
            // 显示存储器的字符地址字节（CA[7:0]）写入到DMDI[7:0]中
            AT7456E_SPI_SendByte(show_char_autoinc.addr_array[i]);            

        }
    }
    
    // 发送停止自动递增标识符
    AT7456E_SPI_SendByte(0xFF);    
    
    // 修改显示字符属性，默认使用该模式下的属性全部一样
    if (show_char_autoinc.attr != 0) {
        for (uint8_t i = 0; i < show_char_autoinc.length; i++) {
            // 写入显示的属性
            AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) | 0x02);// DMAH[1] = 1,DMAH[0]＝x    
            AT7456E_Write_Reg(AT7456E_DMDI, show_char_autoinc.attr);   

            // 写入显示的位置
            AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) & 0x01);// DMAH[1] = 0,DMAH[0]＝x
            AT7456E_Write_Reg(AT7456E_DMAL, (screen_pos + i + 1) & 0xFF);         
                
            
        }         
    }

    return true;
}

/**
 * @brief 自动递增模式中的：8位模式下在屏幕指定位置显示字符
 * @brief 参考手册34页 "在自动递增模式中，写入显示存储器的步骤"
 * @param show_char 显示字符结构体
 * @param show_char.col 起始列号(0-29)
 * @param show_char.row 起始行号(0-15)
 * @param show_char.addr 字符地址(0-254 0xFF需要用来做停止标识符)
 * @param show_char.attr 字符属性 位5: 反色, 位6: 闪烁, 位7: 本地背景控制 (注意：不支持设置位4: CA[8])
 */
bool AT7456E_WriteChar_AutoInc_16bit(AT7456E_ShowChar_AutoInc_t show_char_autoinc)
{
    // 参数检查
    if (show_char_autoinc.col >= OSD_CHARS_PER_ROW || show_char_autoinc.row > 15)
    {
        return false;
    }
    
    // 计算屏幕位置
    uint16_t screen_pos = show_char_autoinc.row * 30 + show_char_autoinc.col; 

    // 检查是否超过显示区域
    if (screen_pos + show_char_autoinc.length > 479) {
        return false;

    }

    // 写入显示的位置
    AT7456E_Write_Reg(AT7456E_DMAH, (screen_pos >> 8) & 0x01);// DMAH[1] = 0,DMAH[0]＝x
    AT7456E_Write_Reg(AT7456E_DMAL, screen_pos & 0xFF);   

    // 确保在8位模式并且开启自动递增模式 DMM[0]=1, DMM[6]=0, DMM[5:3]
    AT7456E_Write_Reg(AT7456E_DMM, AT7456E_Read_Reg(AT7456E_DMAH) | DMM_BIT_AUTO_INC_DISPLAY | show_char_autoinc.attr);     

    for (uint8_t i = 0; i < show_char_autoinc.length; i++) {   
        // 检查发送的字符地址是否为 0xFF
        if (show_char_autoinc.addr_array[i] != 0xFF){
            // 显示存储器的字符地址字节（CA[7:0]）写入到DMDI[7:0]中
            AT7456E_SPI_SendByte(show_char_autoinc.addr_array[i]);        

        }
    }
    
    // 发送停止自动递增标识符
    AT7456E_SPI_SendByte(0xFF);

    return true;
}

/**************** 字库相关函数实现 ****************/

bool AT7456E_Check_Font(void)
{
    bool font_updated = false;
    AT7456E_CharFont_t char_font;
    uint8_t read_char_array[54];
    char_font.data.data_array = read_char_array;   

    // 只检查第一列的16个字符是否一致
    for (uint8_t i = 0; i < 16; i++)
    {
        char_font.addr = i * 16;
        AT7456E_ReadChar_Font(&char_font);
        for (uint8_t i = 0; i < AT7456E_FONT_SIZE; i++) {
            // 检查字库中的内容是否与字库集中一致
            if (char_font.data.data_array[i] != charset_array[char_font.addr][i]) {
                font_updated = true;
                break;
            }
        }

        if (font_updated) {
            break;
        }
    }

    // 检查是否需要更新字库
    if (font_updated) {
        for (uint16_t i = 0; i < 256; i++)
        {
            char_font.addr = i;
            char_font.data.const_data_array = charset_array[i];
            AT7456E_WriteChar_Font(&char_font, AT7456E_WRITE_FONT_AUTO_INC);
        } 
    }   

    return true;
}

/**
 * @brief 从字符存储器读取完整字符数据
 * @brief 参考手册33页 "从字符存储器读取字符字节的步骤"
 * @param read_font 字符字体数据结构指针
 * @return true=成功, false=失败
 */
bool AT7456E_ReadChar_Font(AT7456E_CharFont_t *read_font)
{
    if (read_font == NULL || read_font->addr > 511) {
        return false;
    }

    uint16_t char_addr = read_font->addr;
    // 关闭 OSD 显示
    AT7456E_Set_OSD_State(OSD_SHOW_OFF);

    // 设置字符地址
    uint8_t cmah = (uint8_t)(char_addr & 0xFF);     // CA[7:0]
    uint8_t cmal_bit6 = (char_addr >> 8) & 0x01;    // CA[8]

    AT7456E_Write_Reg(AT7456E_CMAH, cmah);

    // 从NVM读取到镜像RAM
    AT7456E_NVM_Operation(AT7456E_NVM_RAM);

    // 读取 54 字节点阵数据
    for (uint8_t i = 0; i < AT7456E_FONT_SIZE; i++) {
        AT7456E_Write_Reg(AT7456E_CMAL, (cmal_bit6 << 6) | (i & 0x3F));
        read_font->data.data_array[i] = AT7456E_Read_Reg(AT7456E_CMDO);
    }

    //* 恢复 OSD 显示
    AT7456E_Set_OSD_State(OSD_SHOW_ON);

    return true;
}

/**
 * @brief 写入字符字库数据
 * @brief 参考手册33页 "向NVM字符存储器写入字符字节的步骤"
 * @param write_font 字符字库数据结构体指针
 * @param write_font.addr 字符地址(0-511)
 * @param write_font.data_array 字符字体数据数组指针
 * @param write_font_mode 写入模式(普通写入模式或自动递增写入模式)
 */
bool AT7456E_WriteChar_Font(const AT7456E_CharFont_t *write_font, AT7456E_WriteFont_Mode write_font_mode)
{
    // 检查写入字符的地址
    if (write_font == NULL || write_font->addr > 511) {
        return false;
    }

    bool write_status = false;
    
    // 关闭 OSD 显示
    AT7456E_Set_OSD_State(OSD_SHOW_OFF);

    // 清除SRAM内容
    AT7456E_ClearSRAM();
    
    // 写入CMAH[7:0]＝xxH，选择要写入的字符（字库低8位地址：0-255）
    AT7456E_Write_Reg(AT7456E_CMAH, write_font->addr & 0xFF); 
    
    if (write_font_mode == AT7456E_WRITE_FONT_AUTO_INC) {
        // 自动递增模式 CMAH[7:0]＝xxH，CMAL[6]=xH选择要写入的字符（0-511）
        uint8_t addr_h = (write_font->addr >> 8);
        AT7456E_Write_Reg(AT7456E_CMAL, (addr_h << 6));
        AT7456E_Write_Reg(AT7456E_DMM, 0x80);  
        HAL_Delay(1);        
        
    } else {
        // 普通写入模式
        AT7456E_Write_Reg(AT7456E_DMM, 0x00);  
    }	
    
    if (write_font_mode == AT7456E_WRITE_FONT_AUTO_INC) {
        // 注意 别直接整个数据54字节直接丢进去发送，根据手册 18页的介绍 发送一个字节需要一个cs拉低然后拉高的过程，
        // 如果不信邪 可以试试（手动狗头）
        for (uint8_t i = 0; i < AT7456E_FONT_SIZE; i++)
        { 
            AT7456E_SPI_SendByte(write_font->data.const_data_array[i]);            
        }   
        
    } else {
        for (uint8_t i = 0; i < 54; i++)
        { 
            // 写入单个字节到镜像RAM
            AT7456E_Write_Reg(AT7456E_CMDI, write_font->data.const_data_array[i]);            
        }          
    }

    // 将内部镜像RAM中的字库数据写到NVM
	write_status = AT7456E_NVM_Operation(AT7456E_RAM_NVM);
    AT7456E_Read_Reg(AT7456E_DMM);

    //* 恢复 OSD 显示
    AT7456E_Set_OSD_State(OSD_SHOW_ON);    

    return write_status;
}

/**
 * @brief 修改字库中某个字符中的单个字节
 * @brief 参考手册33页中的 “修改已有的字符”
 * @param char_addr 修改字库中字符地址(0-511)
 * @param byte_pos 字符所需修改的字节位置(0-53)
 * @param new_byte 字符所需修改的新的字节值
 * @return 成功返回true
 */
bool AT7456E_ModifyChar_Byte_Font(uint16_t char_addr, uint8_t byte_pos, uint8_t new_byte)
{
    if(char_addr > 511) return false;
    if(byte_pos > 53) return false;
    
    bool status = false;
    
    // 关闭OSD显示
    AT7456E_Set_OSD_State(OSD_SHOW_OFF);
    
    // 设置字符地址
    uint8_t cmah = (uint8_t)(char_addr & 0xFF);      // CA[7:0]
    uint8_t cmal_bit6 = (char_addr >> 8) & 0x01;     // CA[8]
    
    AT7456E_Write_Reg(AT7456E_CMAH, cmah);
    
    // 从NVM读取到镜像RAM
    AT7456E_NVM_Operation(AT7456E_NVM_RAM);
    
    // 设置字节地址 位6：CA[8] 位：5~0：所需修改的字节地址
    AT7456E_Write_Reg(AT7456E_CMAL, (cmal_bit6 << 6) | (byte_pos & 0x3F));

    // 读取当前字节（可选，用于验证）
    // uint8_t old_byte = AT7456E_Read_Reg(AT7456E_CMDO);
    
    // 写入新字节到镜像RAM
    AT7456E_Write_Reg(AT7456E_CMDI, new_byte);
    
    // 7. 写回NVM
    status = AT7456E_NVM_Operation(AT7456E_RAM_NVM);  // 1010xxxx: 写镜像RAM到NVM
    
    // 8. 重新打开OSD
    AT7456E_Set_OSD_State(OSD_SHOW_ON);
    
    return status;
}

/**
 * @brief 修改字符的多个字节,以传入地址为起点 逐一递增修改
 * @brief 参考手册33页 "修改已有的字符"
 * @param char_addr 字符地址(0-511)
 * @param byte_pos 字节位置(0-53)
 * @param modify_array 要修改的字节数组
 * @param len 修改数量
 * @return 成功返回true
 */
bool AT7456E_ModifyChar_ByteArray_Font(uint16_t char_addr, uint8_t byte_pos, const  uint8_t *modify_array, uint8_t len)
{
    if (char_addr > 511 || modify_array == NULL || len == 0 || len > 54 || (byte_pos + len - 1) > 53) {
        return false;

    }
    
    bool status = false;
    
    // 1. 关闭OSD显示
    AT7456E_Set_OSD_State(OSD_SHOW_OFF);
    
    // 3. 计算字符地址
    uint8_t cmah = (uint8_t)(char_addr & 0xFF);
    uint8_t cmal_bit6 = (char_addr >> 8) & 0x01;
    AT7456E_Write_Reg(AT7456E_CMAH, cmah);

    // 4. 从NVM读取到镜像RAM
    AT7456E_NVM_Operation(AT7456E_NVM_RAM);
    
    // 5. 批量修改字节
    for(uint8_t i = 0; i < len; i++) {
        
        // 设置字节地址
        AT7456E_Write_Reg(AT7456E_CMAL, (cmal_bit6 << 6) | ((byte_pos + i) & 0x3F));
        
        // 写入新值
        AT7456E_Write_Reg(AT7456E_CMDI, modify_array[i]);
    }
    
    // 写回NVM
    status = AT7456E_NVM_Operation(AT7456E_RAM_NVM);  // 1010xxxx: 写镜像RAM到NVM
    
    // 重新打开OSD
    AT7456E_Set_OSD_State(OSD_SHOW_ON);
    
    return status;
}

/**************** 工具相关函数实现 ****************/

/**
 * @brief 清除SRAM内容 (可以理解为清屏)
 * 
 */
void AT7456E_ClearSRAM(void)
{
    AT7456E_Write_Reg(AT7456E_DMM,AT7456E_Read_Reg(AT7456E_DMM)|0x04);
    while((AT7456E_Read_Reg(AT7456E_DMM) & 0x04));
    
}

/**
 * @brief 清除显示内容 (在屏幕上显示空字符)
 * 
 */
void AT7456E_Clear_Show(void){
    AT7456E_ShowChar_t show_char;  
    show_char.mode = Show_Char_16bit;
    show_char.addr = 0x00;

    for (int i =0 ;i<16;i++)
    {
        for (int j =0 ;j<30;j++)
        {
            show_char.col = j;
            show_char.row = i;            
            AT7456E_WriteChar(show_char);
        }
    }    
}

/**
 * @brief 设置OSD显示状态
 * @brief 参考手册21页 "VM0寄存器介绍"
 * @param state OSD显示状态
 */
void AT7456E_Set_OSD_State(AT7456E_OSD_Show_State state)
{
    uint8_t current_vm0 = AT7456E_Read_Reg(AT7456E_VM0);

    switch (state) {
        case OSD_SHOW_ON:
            current_vm0 |= 0x08;   // VM0[3]=1 打开osd显示
            break;
        case OSD_SHOW_OFF:
            current_vm0 &= 0xF7;   // VM0[3]=0 关闭osd显示
            break;
        default:
            // 无效状态，不操作
            return;
    }

    AT7456E_Write_Reg(AT7456E_VM0, current_vm0);
    while (AT7456E_Read_Reg(AT7456E_STAT) & 0x20);  // 等待STAT[5]=0  
}

/**
 * @brief 执行NVM操作（读/写）并等待完成
 * @brief 参考手册28页 "CMM寄存器介绍"
 * @param operation 操作类型：AT7456E_NVM_RAM 或 AT7456E_RAM_NVM
 * @return true=成功, false=超时失败
 */
bool AT7456E_NVM_Operation(uint8_t operation)
{
    uint8_t status;
    uint32_t timeout = 100;  // 100ms超时

    // 字符存储器状态是否处于可读状态
    while(AT7456E_Read_Reg(AT7456E_STAT) & 0x20);  // 等待STAT[5]=0    
    
    // 发送操作命令
    switch(operation) {
        case AT7456E_NVM_RAM:   // 从NVM读取到镜像RAM
            AT7456E_Write_Reg(AT7456E_CMM, AT7456E_NVM_RAM);  // 0101xxxx: 读NVM到镜像RAM
            break;
        case AT7456E_RAM_NVM:  // 从镜像RAM写入到NVM
             AT7456E_Write_Reg(AT7456E_CMM, AT7456E_RAM_NVM);  // 1010xxxx: 写镜像RAM到NVM
            break;
        default:
            return false;  // 无效操作
    }
    
    // 等待操作完成
    do {
        status = AT7456E_Read_Reg(AT7456E_STAT);
        HAL_Delay(1);
        if(--timeout == 0) {
            break;
        }
    } while(status & 0x20);  // 等待STAT[5]=0    
    
    return true;
}

void AT7456E_Show_Full_Icon(bool hight_addr)
{
    uint16_t addr_start = 0;
    if (hight_addr) {
        addr_start = 256;
    }
    AT7456E_ShowChar_t show_char;  
    show_char.mode = Show_Char_8bit;
    show_char.attr = DMDI_ATTR_NONE;
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            show_char.col = j + 1;
            show_char.row = i;
            show_char.addr = addr_start + i * 16 + j;                
            AT7456E_WriteChar(show_char);
        }
    }     



}

/**************** 测试相关函数实现 ****************/

void AT7456E_TEST(void)
{  
    uint8_t test_flag = 0x04;
    static bool show_mode = false;      

    HAL_GPIO_TogglePin(LED_PB12_GPIO_Port, LED_PB12_Pin);
    if (HAL_GPIO_ReadPin(LED_PB12_GPIO_Port, LED_PB12_Pin) == GPIO_PIN_SET) {   
        show_mode = false;

    } else {
        show_mode = true;

    }

    // 测试不同模式下的非自动递增全字符显示 
    if (test_flag & (1<<0)) {   
        AT7456E_WriteChar_Test(show_mode);
        HAL_Delay(1500);     
    }  
 
    // 测试不同模式下的自动递增显示
    if (test_flag & (1<<1)) {
        if (show_mode == true) { 
            AT7456E_WriteChar_AutoInc_Test();

        } else {
            // AT7456E_ClearSRAM();
            
        }         
    }

    // 测试字库相关内容
    if (test_flag & (1<<2)) {
        AT7456E_ModifyChar_Byte_TEST();
    }
}

// 测试普通显示模式
void AT7456E_WriteChar_Test(bool status)
{
    uint8_t write_test_flag = 0x01;
    AT7456E_ShowChar_t show_char;  

    // 全字符集显示测试
    if (write_test_flag & (1<<0)) { 
        if (status) {
            // 16位模式测试 字符属性需要通过设置DMM寄存器来实现
            show_char.mode = Show_Char_16bit;
            show_char.attr = DMM_NONE;        
            for (int i = 0; i < 16; i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    show_char.col = j + 1;
                    show_char.row = i;
                    show_char.addr = i * 16 + j;                
                    AT7456E_WriteChar(show_char);
                }
            }         

        } else {
            // 8位模式测试 配置DMDI寄存器实现单字符属性设置
            show_char.mode = Show_Char_8bit;
            show_char.attr = DMDI_ATTR_NONE;
            for (int i = 0; i < 16; i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    show_char.col = j + 1;
                    show_char.row = i;
                    show_char.addr = 256 + i * 16 + j;                
                    AT7456E_WriteChar(show_char);
                }
            }        
        }        
    }

    // 测试显示特定字符
    if (write_test_flag & (1<<1)) {
        // 8位工作模式 显示默认属性
        uint8_t show_addr_array[5] = {0xEA,0xEB,0xEC,0xED,0x21};// 你好世界！
        show_char.mode = Show_Char_8bit;
        show_char.attr = DMDI_ATTR_NONE;
        for (uint8_t i =0 ;i< sizeof(show_addr_array);i++){            
            show_char.col = 10 + i;
            show_char.row = 3;
            show_char.addr = show_addr_array[i];
            AT7456E_WriteChar(show_char);
        }  

        // 8位工作模式 可单独设置显示属性
        uint8_t show_addr_array1[5] = {0xEA,0xEB,0xEC,0xED,0x21};// 你好世界！
        uint8_t show_attr_array1[5] = {DMDI_ATTR_NONE,DMDI_ATTR_INV,DMDI_ATTR_BLK,DMDI_ATTR_LBC,DMDI_ATTR_NONE};
        show_char.mode = Show_Char_8bit;
        for (uint8_t i =0 ;i< sizeof(show_addr_array1);i++){
            show_char.attr = show_attr_array1[i];
            show_char.col = 10 + i;
            show_char.row = 4;
            show_char.addr = show_addr_array1[i];
            AT7456E_WriteChar(show_char);
        }

        // 16位工作模式 显示默认属性
        uint8_t show_addr_array2[5] = {0xEA,0xEB,0xEC,0xED,0x21};// 你好世界！
        show_char.mode = Show_Char_16bit;
        show_char.attr = DMM_NONE;
        for (uint8_t i =0 ;i< sizeof(show_addr_array2);i++){            
            show_char.col = 10 + i;
            show_char.row = 5;
            show_char.addr = show_addr_array2[i];
            AT7456E_WriteChar(show_char);
        }     
        
        // 16位工作模式 统一设置显示属性
        uint8_t show_addr_array3[5] = {0xEA,0xEB,0xEC,0xED,0x21};// 你好世界！
        show_char.mode = Show_Char_16bit;
        show_char.attr = DMM_BIT_ATTR_INV;
        for (uint8_t i =0 ;i< sizeof(show_addr_array3);i++){            
            show_char.col = 10 + i;
            show_char.row = 6;
            show_char.addr = show_addr_array3[i];
            AT7456E_WriteChar(show_char);
        }        

    }

                 
}

// 测试自动递增显示模式
void AT7456E_WriteChar_AutoInc_Test(void)
{    
    AT7456E_ShowChar_AutoInc_t show_char_autoinc;   

    // 8位模式测试 默认使用该模式区域相同显示属性
    uint8_t show_array1[] = {'H', 'E', 'L', 'L', 'O', ',', 'W', 'O', 'R', 'L', 'D', '!'};      
    show_char_autoinc.addr_array = show_array1;
    show_char_autoinc.length = sizeof(show_array1);
    show_char_autoinc.attr = DMDI_ATTR_BLK ; // 闪烁(BLK)
    show_char_autoinc.col = 3;
    show_char_autoinc.row = 3;    
    show_char_autoinc.mode = Show_Char_8bit;        
    AT7456E_WriteChar_AutoInc(show_char_autoinc);    

    // 16位模式测试  
    // 1 与 ‘1’ 多代表的地址不一样。 1就是地址为1，‘1’是ASCII码中的1，其十进制值为49(0X31)
    // uint8_t show_array2[6] = {1, 2, 3, 5, 6, 7};
    uint8_t show_array2[6] = {'1', '2', '3', '5', '6', '7'};
    show_char_autoinc.addr_array = show_array2;
    show_char_autoinc.length = sizeof(show_array2);
    show_char_autoinc.attr = DMM_NONE ; // 闪烁(BLK)
    show_char_autoinc.col = 3;
    show_char_autoinc.row = 4;    
    show_char_autoinc.mode = Show_Char_16bit;     
    AT7456E_WriteChar_AutoInc(show_char_autoinc);
}

// 测试修改字库
void AT7456E_ModifyChar_Byte_TEST(void)
{
    //  一个字符是像素大小是 12 * 18
    //  一个像素需要2 bit 表示 所以用 6 * 9 byte即可表示
    // 0x00：黑色 0xAA：白色 0x55 透明

    uint8_t font_test_flag = 0x01;
    static bool font_updata = false;

    // 单个字节修改测试
    if (font_test_flag & (1<<0)&& !font_updata) { 
        for (int i = 0; i < 6; i++) {
            // 将字库中地址 0x20 的第4行全部换成黑色
            AT7456E_ModifyChar_Byte_Font(0x20, 6 * 3 + i, 0x00);   
        }
        font_updata = true;         
    }   
    
    // 数组修改测试
    if (font_test_flag & (1<<1)&& !font_updata) { 
        uint8_t modify_data[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; 
        // 将字库中地址 0x20 的第7行全部换成白色
        AT7456E_ModifyChar_ByteArray_Font(0x20, 6 * 6, modify_data, 6);
        font_updata = true; 
    
    }
    
    // 修改整一个字符
    if (font_test_flag & (1<<2)&& !font_updata) { 
        AT7456E_ModifyChar_ByteArray_Font(0x20, 0, charset_array_ni, 54); 
        font_updata = true; 

    }   

    // 读取字库某个地址中的内容
    if (font_test_flag & (1<<3)) { 
        AT7456E_CharFont_t font;
        font.addr = 0x1FF;
        AT7456E_ReadChar_Font(&font);        
        
    }

    // 测试是否需要更新字库
    if (font_test_flag & (1<<4) && !font_updata) { 
        // 手动修改 字库中0x00 储存的内容，使得触发写字库
        uint8_t modify_data[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};         
        AT7456E_ModifyChar_ByteArray_Font(0x20, 6 * 6, modify_data, 6); // 将字库中地址0xFF的第7行全部换成白色
              
        AT7456E_Check_Font();     
        font_updata = true; 
        
    }   
    
    AT7456E_Show_Full_Icon(false);   
    HAL_Delay(500);
}
