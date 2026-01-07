# -*- coding: gbk -*-

"""
mcm_to_h.py
-----------------
将 MCM 格式的字体数据 转换为适用于 AT7456E 的 C 头文件。

处理说明：
- MCM 文件内包含 512 个字符，每字符占 64 行点阵数据。
- 实际有效的点阵行为前 54 行（FONT_LINES），其余为填充或保留。
- 每行由若干二进制字符组成（如 "01010101"），脚本把每行视作一个字节并转换为十六进制表示。
- 最终输出两个 256 项的二维数组 `font_array0` 和 `font_array1`，每项包含 54 个字节。
"""

import os

# 工作目录（当前脚本所在目录）
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# 输入 MCM 文件（原始点阵数据）和输出 C 头文件路径
INPUT_MCM = os.path.join(BASE_DIR, "bfstyle_me.MCM")
OUTPUT_H = os.path.join(BASE_DIR, "build", "AT7456E_font.h")

# 确保输出目录存在
os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)

# MCM 文件相关常量
CHARS_TOTAL = 512        # 总字符数（两个 256 表）
LINES_PER_CHAR = 64      # 每个字符在 MCM 中占的行数
FONT_LINES = 54          # 实际有效的点阵行数（取前 54 行）
BYTES_PER_LINE = 6       # 仅用于格式化输出（每行写入 6 个字节以便可读性）
WRITE_BACK_FALG = False  # 是否需要写后256字符数组

def bin_to_hex(b):
    """将二进制字符串（如 '01010101'）转换为十六进制字节字符串（如 '0x55'）。

    参数:
        b (str): 由 '0' 和 '1' 组成的字符串，长度应为 8（表示一字节）。

    返回:
        str: 形如 '0x00' 的十六进制字符串（大写字母）。
    """
    return f"0x{int(b, 2):02X}"


# 读取输入文件并去掉空行（同时剔除首尾空白）
with open(INPUT_MCM, "r") as f:
    lines = [l.strip() for l in f if l.strip()]

# MCM 文件通常在第一行包含头部描述信息，跳过该行
lines = lines[1:]

# 验证文件行数是否符合预期结构，便于早期报错定位
expected_lines = CHARS_TOTAL * LINES_PER_CHAR
if len(lines) < expected_lines:
    raise ValueError("文件行数不足，结构不符合预期")

# 将每个字符的前 FONT_LINES 行提取并转换为十六进制字节表示
chars = []  # chars 将成为一个 512 x 54 的列表（每个子项为 54 个 '0x..' 字符）

for c in range(CHARS_TOTAL):
    base = c * LINES_PER_CHAR
    # font_bits 包含该字符的有效点阵行（只取前 54 行）
    font_bits = lines[base : base + FONT_LINES]

    # 将每一行二进制字符串转换为十六进制字节表示
    char_bytes = [bin_to_hex(b) for b in font_bits]
    chars.append(char_bytes)


# 生成 C 头文件，包含两个 256 项的二维数组，便于在设备端直接使用
with open(OUTPUT_H, "w") as f:
    # 头文件保护宏和标准类型包含
    f.write("#ifndef AT7456E_FONT_H\n")
    f.write("#define AT7456E_FONT_H\n\n")
    f.write("#ifdef __cplusplus\n")
    f.write("extern \"C\" {\n")
    f.write("#endif\n")
    f.write("#include <stdint.h>\n\n")

    # 写入前 256 个字符（font_set0）
    f.write("// 前 256 个字符（地址 0x00 - 0xFF）\n")
    f.write("const uint8_t charset_array[256][54] = {\n")
    for i in range(256):
        # 每个字符前写一行注释，指出其在总体中的地址（便于对照）
        f.write(f"    // 地址 0x{i:02X}\n")
        f.write("    {\n")

        # 为了可读性，将 54 个字节分成若干行，每行 BYTES_PER_LINE 个字节
        for j in range(0, 54, BYTES_PER_LINE):
            line = ", ".join(chars[i][j:j+BYTES_PER_LINE])
            f.write(f"        {line},\n")

        f.write("    },\n")
    f.write("};\n\n")
    
    if WRITE_BACK_FALG:
    # 写入后 256 个字符（font_set1）
        f.write("// 后 256 个字符（地址 0x100 - 0x1FF）\n")
        f.write("const uint8_t charset_array1[256][54] = {\n")
        for i in range(256, 512):
            f.write(f"    // 地址 0x{i:02X}\n")
            f.write("    {\n")

            for j in range(0, 54, BYTES_PER_LINE):
                line = ", ".join(chars[i][j:j+BYTES_PER_LINE])
                f.write(f"        {line},\n")

            f.write("    },\n")
        f.write("};\n\n")

    f.write("#ifdef __cplusplus\n")
    f.write("}\n")
    f.write("#endif\n")
    f.write("#endif /* __AT7456E_FONTH */\n") 

print("转换完成：", OUTPUT_H)
