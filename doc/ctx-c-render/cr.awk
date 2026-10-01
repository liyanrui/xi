#!/usr/bin/gawk -f

BEGIN {
    keyword_color = "darkblue"
    func_color    = "magenta"
    string_color  = "darkgreen"
    comment_color = "darkyellow"
    number_color  = "darkred"

    kw["auto"]=1; kw["break"]=1; kw["case"]=1; kw["char"]=1
    kw["const"]=1; kw["continue"]=1; kw["default"]=1; kw["do"]=1
    kw["double"]=1; kw["else"]=1; kw["enum"]=1; kw["extern"]=1
    kw["float"]=1; kw["for"]=1; kw["goto"]=1; kw["if"]=1
    kw["int"]=1; kw["long"]=1; kw["register"]=1; kw["return"]=1
    kw["short"]=1; kw["signed"]=1; kw["sizeof"]=1; kw["static"]=1
    kw["struct"]=1; kw["switch"]=1; kw["typedef"]=1; kw["union"]=1
    kw["unsigned"]=1; kw["void"]=1; kw["volatile"]=1; kw["while"]=1

    # 全局状态
    in_code    = 0    # 是否在代码块内部
    in_multi   = 0    # 多行注释中
    in_string  = 0    # 双引号字符串中
    in_char    = 0    # 单引号字符中
    escape_state = 0  # 转义状态
    in_header  = 0    # 是否在块头缓冲模式
    header_text = ""  # 缓冲的块头文本
}

function begin_color(color) { return "/BTEX\\color[" color "]{" }
function end_color()         { return "}/ETEX" }

function escape_comment_char(c) {
    if (c == " ") return "\\space{}"
    if (c == "{") return "\\textbraceleft{}"
    if (c == "}") return "\\textbraceright{}"
    if (c == "\\") return "\\textbackslash{}"
    if (c == "%") return "\\textpercent{}"
    if (c == "#") return "\\texthash{}"
    if (c == "|") return "\\textbar{}"
    if (c == "$") return "\\textdollar{}"
    if (c == "~") return "\\textasciitilde{}"
    if (c == "^") return "\\textasciicircum{}"
    if (c == "_") return "\\textunderscore{}"
    if (c == "&") return "\\textampersand{}"
    return c
}

function escape_string_char(c) {
    if (c == "{") return "\\textbraceleft{}"
    if (c == "}") return "\\textbraceright{}"
    if (c == "\\") return "\\textbackslash{}"
    if (c == "%") return "\\textpercent{}"
    if (c == "#") return "\\texthash{}"
    if (c == "|") return "\\textbar{}"
    if (c == "$") return "\\textdollar{}"
    if (c == "~") return "\\textasciitilde{}"
    if (c == "^") return "\\textasciicircum{}"
    if (c == "_") return "\\textunderscore{}"
    if (c == "&") return "\\textampersand{}"
    return c
}

function escape_light(str) {
    gsub(/{/, "\\textbraceleft{}", str)
    gsub(/}/, "\\textbraceright{}", str)
    gsub(/\\/, "\\textbackslash{}", str)
    gsub(/%/, "\\textpercent{}", str)
    gsub(/#/, "\\texthash{}", str)
    return str
}

{
    # ---------- 处理块头缓冲 ----------
    if (in_header) {
        if ($0 ~ /#/) {
            # 遇到包含 # 的行，块头结束
            header_text = header_text $0   # 最后一行不加换行，后续 print 会输出
            print header_text
            header_text = ""
            in_header = 0
            in_code = 1
            next
        } else {
            # 继续缓冲
            header_text = header_text $0 "\n"
            next
        }
    }

    # ---------- 代码块结束（单独的 @） ----------
    if (in_code && $0 ~ /^@[ \t]*$/) {
        in_code = 0
        print $0
        next
    }

    # ---------- 代码块内部 ----------
    if (in_code) {
        # 引用行（允许缩进）原样输出
        if ($0 ~ /^[ \t]*#[^@]*@[ \t]*$/) {
            print $0
            next
        }
        # 否则进行 C 语法高亮
        highlight_line($0)
        next
    }

    # ---------- 块外：检测块开始 ----------
    if ($0 ~ /^@/) {
        if ($0 ~ /#/) {
            # 单行块头（本行内包含 #），直接进入代码模式
            print $0
            in_code = 1
            next
        } else {
            # 跨行块头开始
            in_header = 1
            header_text = $0 "\n"
            next
        }
    }

    # ---------- 非代码、非块头：原样输出 ----------
    print $0
}

# 行末处理：若文件结束时仍在缓冲模式，输出剩余内容
END {
    if (in_header) {
        print header_text
    }
}

function highlight_line(line,    i, c, out, token, j, nc) {
    out = ""
    if (in_multi)  { out = out begin_color(comment_color) }
    if (in_string) { out = out begin_color(string_color) }
    if (in_char)   { out = out begin_color(string_color) }

    i = 1
    while (i <= length(line)) {
        c = substr(line, i, 1)

        # --- 多行注释 ---
        if (in_multi) {
            if (c == "*" && i+1 <= length(line) && substr(line, i+1, 1) == "/") {
                out = out escape_comment_char(c) escape_comment_char("/") end_color()
                in_multi = 0
                i += 2
                continue
            } else {
                out = out escape_comment_char(c)
                i++
                continue
            }
        }

        # --- 双引号字符串 ---
        if (in_string) {
            if (escape_state) {
                out = out escape_string_char(c)
                escape_state = 0
            } else if (c == "\\") {
                out = out escape_string_char(c)
                escape_state = 1
            } else if (c == "\"") {
                out = out escape_string_char(c) end_color()
                in_string = 0
            } else {
                out = out escape_string_char(c)
            }
            i++
            continue
        }

        # --- 单引号字符 ---
        if (in_char) {
            if (escape_state) {
                out = out escape_string_char(c)
                escape_state = 0
            } else if (c == "\\") {
                out = out escape_string_char(c)
                escape_state = 1
            } else if (c == "'") {
                out = out escape_string_char(c) end_color()
                in_char = 0
            } else {
                out = out escape_string_char(c)
            }
            i++
            continue
        }

        # --- 普通代码 ---
        if (c == "/") {
            nc = (i+1 <= length(line) ? substr(line, i+1, 1) : "")
            if (nc == "/") {
                out = out begin_color(comment_color) escape_comment_char("/") escape_comment_char("/")
                i += 2
                while (i <= length(line)) {
                    out = out escape_comment_char(substr(line, i, 1))
                    i++
                }
                out = out end_color()
                break
            } else if (nc == "*") {
                in_multi = 1
                out = out begin_color(comment_color) escape_comment_char("/") escape_comment_char("*")
                i += 2
                continue
            } else {
                out = out c
            }
        } else if (c == "\"") {
            in_string = 1
            out = out begin_color(string_color) escape_string_char(c)
        } else if (c == "'") {
            in_char = 1
            out = out begin_color(string_color) escape_string_char(c)
        } else if (c ~ /[a-zA-Z_]/) {
            token = ""
            while (i <= length(line) && substr(line, i, 1) ~ /[a-zA-Z0-9_]/) {
                token = token substr(line, i, 1)
                i++
            }
            i--
            j = i + 1
            while (j <= length(line) && substr(line, j, 1) ~ /[ \t]/) { j++ }
            if (j <= length(line) && substr(line, j, 1) == "(" && !(token in kw)) {
                out = out begin_color(func_color) escape_light(token) end_color()
            } else if (token in kw) {
                out = out begin_color(keyword_color) escape_light(token) end_color()
            } else {
                out = out token
            }
        } else if (c ~ /[0-9]/) {
            token = ""
            while (i <= length(line) && substr(line, i, 1) ~ /[0-9a-fA-FxX\.\+\-eE]/) {
                token = token substr(line, i, 1)
                i++
            }
            i--
            out = out begin_color(number_color) escape_light(token) end_color()
        } else {
            out = out c
        }
        i++
    }

    if (in_multi) { out = out end_color() }
    if (in_string || in_char) { out = out end_color() }
    print out
}
