#!/usr/bin/env python3

import sys
import re

# 颜色定义
KEYWORD_COLOR = "darkblue"
FUNC_COLOR    = "magenta"
STRING_COLOR  = "darkgreen"
COMMENT_COLOR = "darkyellow"
NUMBER_COLOR  = "darkred"

# C 关键字集合
KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while"
}

# 全局状态（跨行保持）
in_code = False          # 是否在代码块内
in_multi = False         # 多行注释中
in_string = 0            # 0=无, 1=双引号字符串, 2=单引号字符
escape_state = False     # 字符串/字符内的转义状态
in_header = False        # 是否在跨行块头缓冲中
header_lines = []        # 缓冲的块头行

def begin_color(color):
    return f"/BTEX\\color[{color}]{{"

def end_color():
    return "}/ETEX"

# 注释内字符转义（空格 → \space{}，保留缩进）
def escape_comment_char(c):
    if c == " ":
        return "\\space{}"
    if c == "{": return "\\textbraceleft{}"
    if c == "}": return "\\textbraceright{}"
    if c == "\\": return "\\textbackslash{}"
    if c == "%": return "\\textpercent{}"
    if c == "#": return "\\texthash{}"
    if c == "|": return "\\textbar{}"
    if c == "$": return "\\textdollar{}"
    if c == "~": return "\\textasciitilde{}"
    if c == "^": return "\\textasciicircum{}"
    if c == "_": return "\\textunderscore{}"
    if c == "&": return "\\textampersand{}"
    return c

# 字符串/字符内特殊字符转义（不转义空格）
def escape_string_char(c):
    if c == "{": return "\\textbraceleft{}"
    if c == "}": return "\\textbraceright{}"
    if c == "\\": return "\\textbackslash{}"
    if c == "%": return "\\textpercent{}"
    if c == "#": return "\\texthash{}"
    if c == "|": return "\\textbar{}"
    if c == "$": return "\\textdollar{}"
    if c == "~": return "\\textasciitilde{}"
    if c == "^": return "\\textasciicircum{}"
    if c == "_": return "\\textunderscore{}"
    if c == "&": return "\\textampersand{}"
    return c

# 标识符/关键字/数字轻量转义（仅处理破坏分组的字符）
def escape_light(s):
    s = s.replace("{", "\\textbraceleft{}")
    s = s.replace("}", "\\textbraceright{}")
    s = s.replace("\\", "\\textbackslash{}")
    s = s.replace("%", "\\textpercent{}")
    s = s.replace("#", "\\texthash{}")
    return s

def highlight_line(line):
    """对单行 C 代码进行语法高亮，返回包含 /BTEX 标记的字符串"""
    global in_multi, in_string, escape_state
    out = []
    # 跨行状态开始时打开相应颜色
    if in_multi:
        out.append(begin_color(COMMENT_COLOR))
    if in_string == 1 or in_string == 2:
        out.append(begin_color(STRING_COLOR))

    i = 0
    n = len(line)
    while i < n:
        c = line[i]

        # ---------- 多行注释内部 ----------
        if in_multi:
            if c == '*' and i+1 < n and line[i+1] == '/':
                out.append(escape_comment_char(c))
                out.append(escape_comment_char('/'))
                out.append(end_color())
                in_multi = False
                i += 2
                continue
            else:
                out.append(escape_comment_char(c))
                i += 1
                continue

        # ---------- 双引号字符串内部 ----------
        if in_string == 1:
            if escape_state:
                out.append(escape_string_char(c))
                escape_state = False
            elif c == '\\':
                out.append(escape_string_char(c))
                escape_state = True
            elif c == '"':
                out.append(escape_string_char(c))
                out.append(end_color())
                in_string = 0
            else:
                out.append(escape_string_char(c))
            i += 1
            continue

        # ---------- 单引号字符内部 ----------
        if in_string == 2:
            if escape_state:
                out.append(escape_string_char(c))
                escape_state = False
            elif c == '\\':
                out.append(escape_string_char(c))
                escape_state = True
            elif c == "'":
                out.append(escape_string_char(c))
                out.append(end_color())
                in_string = 0
            else:
                out.append(escape_string_char(c))
            i += 1
            continue

        # ---------- 正常代码状态 ----------
        # 1) 注释、字符串、字符的开始
        if c == '/':
            if i+1 < n:
                nc = line[i+1]
                if nc == '/':
                    # 单行注释：打开颜色，逐字符转义剩余部分
                    out.append(begin_color(COMMENT_COLOR))
                    out.append(escape_comment_char('/'))
                    out.append(escape_comment_char('/'))
                    i += 2
                    while i < n:
                        out.append(escape_comment_char(line[i]))
                        i += 1
                    out.append(end_color())
                    break   # 整行结束
                elif nc == '*':
                    # 多行注释开始
                    in_multi = True
                    out.append(begin_color(COMMENT_COLOR))
                    out.append(escape_comment_char('/'))
                    out.append(escape_comment_char('*'))
                    i += 2
                    continue
                else:
                    out.append(c)   # 除号
            else:
                out.append(c)
        elif c == '"':
            in_string = 1
            out.append(begin_color(STRING_COLOR))
            out.append(escape_string_char(c))
        elif c == "'":
            in_string = 2
            out.append(begin_color(STRING_COLOR))
            out.append(escape_string_char(c))
        # 2) 标识符 / 关键字 / 函数名
        elif c.isalpha() or c == '_':
            token = []
            while i < n and (line[i].isalnum() or line[i] == '_'):
                token.append(line[i])
                i += 1
            i -= 1   # 回退，因为循环会自增
            token_str = ''.join(token)
            # 向前跳过空白，看下一个非空白字符是不是 '('
            j = i + 1
            while j < n and line[j] in (' ', '\t'):
                j += 1
            if j < n and line[j] == '(' and token_str not in KEYWORDS:
                out.append(begin_color(FUNC_COLOR) + escape_light(token_str) + end_color())
            elif token_str in KEYWORDS:
                out.append(begin_color(KEYWORD_COLOR) + escape_light(token_str) + end_color())
            else:
                out.append(token_str)   # 普通标识符不加颜色
        # 3) 数字
        elif c.isdigit():
            token = []
            while i < n and (line[i].isdigit() or line[i] in 'xXabcdefABCDEF.+-eE'):
                token.append(line[i])
                i += 1
            i -= 1
            token_str = ''.join(token)
            out.append(begin_color(NUMBER_COLOR) + escape_light(token_str) + end_color())
        # 4) 其他字符（运算符、标点、空格等）
        else:
            out.append(c)
        i += 1

    # 行末：如果跨行状态还在，关闭颜色（下一行会再打开）
    if in_multi:
        out.append(end_color())
    if in_string in (1, 2):
        out.append(end_color())

    return ''.join(out)

def process_line(line):
    """处理一行输入，返回（或打印）转换后的行"""
    global in_code, in_header, header_lines

    # ---------- 处理跨行块头缓冲 ----------
    if in_header:
        if '#' in line:
            # 块头结束：将缓冲的所有行和本行合并，输出完整块头
            header_lines.append(line)
            full_header = '\n'.join(header_lines)
            # 注意：原 awk 版本最后一行不加换行，print 输出后自带换行
            # 这里我们直接打印完整的块头（保持原始换行）
            print(full_header)
            header_lines.clear()
            in_header = False
            in_code = True
            return
        else:
            # 继续缓冲
            header_lines.append(line)
            return

    # ---------- 代码块结束：单独的 @（允许前后空格） ----------
    if in_code and re.fullmatch(r'@[ \t]*', line):
        in_code = False
        print(line)
        return

    # ---------- 代码块内部 ----------
    if in_code:
        # 引用行（允许缩进）原样输出
        if re.match(r'^[ \t]*#[^@]*@[ \t]*$', line):
            print(line)
            return
        # 否则进行 C 语法高亮
        highlighted = highlight_line(line)
        print(highlighted)
        return

    # ---------- 代码块外部：检测块开始 ----------
    if line.startswith('@'):
        if '#' in line:
            # 单行块头：直接输出并进入代码模式
            print(line)
            in_code = True
        else:
            # 跨行块头开始
            in_header = True
            header_lines.append(line)
        return

    # ---------- 非代码、非块头：原样输出 ----------
    print(line)

def main():
    global in_header, header_lines
    for line in sys.stdin:
        # 去掉尾随换行符，保留其他空白
        line = line.rstrip('\n')
        process_line(line)
    # 文件结束时，如果还在缓冲块头，输出剩余内容
    if in_header and header_lines:
        print('\n'.join(header_lines))

if __name__ == "__main__":
    main()
