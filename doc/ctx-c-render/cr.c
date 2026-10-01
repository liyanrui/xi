/*
 * litprog_highlight_opt.c
 * 优化版：使用输出缓冲区，减少系统调用。
 * 编译: gcc -O2 -o litprog_highlight litprog_highlight_opt.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ---------- 颜色定义 ---------- */
#define KEYWORD_COLOR "darkblue"
#define FUNC_COLOR    "magenta"
#define STRING_COLOR  "darkgreen"
#define COMMENT_COLOR "darkyellow"
#define NUMBER_COLOR  "darkred"

/* ---------- 输出缓冲区 ---------- */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static inline void buf_init(Buf *b) {
    b->len = 0;
    b->cap = 256;
    b->data = malloc(b->cap);
}

static inline void buf_ensure(Buf *b, size_t extra) {
    if (b->len + extra >= b->cap) {
        b->cap = b->cap * 2 + extra;
        b->data = realloc(b->data, b->cap);
        if (!b->data) { perror("realloc"); exit(EXIT_FAILURE); }
    }
}

static inline void buf_append(Buf *b, const char *s, size_t len) {
    buf_ensure(b, len);
    memcpy(b->data + b->len, s, len);
    b->len += len;
}

static inline void buf_append_str(Buf *b, const char *s) {
    buf_append(b, s, strlen(s));
}

static inline void buf_append_char(Buf *b, char c) {
    buf_ensure(b, 1);
    b->data[b->len++] = c;
}

static inline void buf_free(Buf *b) {
    free(b->data);
}

/* ---------- C 关键字集合 ---------- */
static const char *keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while", NULL
};

static bool is_keyword(const char *word, size_t len) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strlen(keywords[i]) == len && memcmp(keywords[i], word, len) == 0)
            return true;
    }
    return false;
}

/* ---------- 全局状态 ---------- */
static bool in_code = false;
static bool in_multi = false;
static int  in_string = 0;         /* 0:无, 1:双引号, 2:单引号 */
static bool escape_state = false;
static bool in_header = false;

/* 跨行块头缓冲（仍保留，因为行数少） */
static char *header_buf = NULL;
static size_t header_len = 0;
static size_t header_cap = 0;

static void add_to_header(const char *line) {
    size_t len = strlen(line);
    if (header_len + len + 2 > header_cap) {
        header_cap = header_cap ? header_cap * 2 : 1024;
        header_buf = realloc(header_buf, header_cap);
        if (!header_buf) { perror("realloc"); exit(EXIT_FAILURE); }
    }
    if (header_len > 0) header_buf[header_len++] = '\n';
    memcpy(header_buf + header_len, line, len);
    header_len += len;
}

static void flush_header(void) {
    if (header_buf) {
        fwrite(header_buf, 1, header_len, stdout);
        putchar('\n');
        free(header_buf);
        header_buf = NULL;
        header_len = header_cap = 0;
    }
    in_header = false;
}

/* ---------- 转义函数（直接写入 Buf）---------- */

static void escape_comment_char(Buf *b, char c) {
    switch (c) {
        case ' ': buf_append_str(b, "\\space{}"); break;
        case '{': buf_append_str(b, "\\textbraceleft{}"); break;
        case '}': buf_append_str(b, "\\textbraceright{}"); break;
        case '\\': buf_append_str(b, "\\textbackslash{}"); break;
        case '%': buf_append_str(b, "\\textpercent{}"); break;
        case '#': buf_append_str(b, "\\texthash{}"); break;
        case '|': buf_append_str(b, "\\textbar{}"); break;
        case '$': buf_append_str(b, "\\textdollar{}"); break;
        case '~': buf_append_str(b, "\\textasciitilde{}"); break;
        case '^': buf_append_str(b, "\\textasciicircum{}"); break;
        case '_': buf_append_str(b, "\\textunderscore{}"); break;
        case '&': buf_append_str(b, "\\textampersand{}"); break;
        default: buf_append_char(b, c); break;
    }
}

static void escape_string_char(Buf *b, char c) {
    switch (c) {
        case '{': buf_append_str(b, "\\textbraceleft{}"); break;
        case '}': buf_append_str(b, "\\textbraceright{}"); break;
        case '\\': buf_append_str(b, "\\textbackslash{}"); break;
        case '%': buf_append_str(b, "\\textpercent{}"); break;
        case '#': buf_append_str(b, "\\texthash{}"); break;
        case '|': buf_append_str(b, "\\textbar{}"); break;
        case '$': buf_append_str(b, "\\textdollar{}"); break;
        case '~': buf_append_str(b, "\\textasciitilde{}"); break;
        case '^': buf_append_str(b, "\\textasciicircum{}"); break;
        case '_': buf_append_str(b, "\\textunderscore{}"); break;
        case '&': buf_append_str(b, "\\textampersand{}"); break;
        default: buf_append_char(b, c); break;
    }
}

static void escape_light(Buf *b, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        switch (c) {
            case '{': buf_append_str(b, "\\textbraceleft{}"); break;
            case '}': buf_append_str(b, "\\textbraceright{}"); break;
            case '\\': buf_append_str(b, "\\textbackslash{}"); break;
            case '%': buf_append_str(b, "\\textpercent{}"); break;
            case '#': buf_append_str(b, "\\texthash{}"); break;
            default: buf_append_char(b, c); break;
        }
    }
}

/* ---------- 字符分类（避免 locale 干扰）---------- */
static inline bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline bool is_ident_cont(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}
static inline bool is_num_part(char c) {
    return (c >= '0' && c <= '9') ||
           c == 'x' || c == 'X' ||
           (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
           c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E';
}

/* ---------- 语法高亮主函数（写入 Buf）---------- */
static void highlight_line(const char *line, size_t len, Buf *out) {
    if (in_multi)
        buf_append_str(out, "/BTEX\\color[" COMMENT_COLOR "]{");
    if (in_string == 1 || in_string == 2)
        buf_append_str(out, "/BTEX\\color[" STRING_COLOR "]{");

    size_t i = 0;
    while (i < len) {
        char c = line[i];

        /* 多行注释内部 */
        if (in_multi) {
            if (c == '*' && i+1 < len && line[i+1] == '/') {
                escape_comment_char(out, '*');
                escape_comment_char(out, '/');
                buf_append_str(out, "}/ETEX");
                in_multi = false;
                i += 2;
                continue;
            } else {
                escape_comment_char(out, c);
                i++;
                continue;
            }
        }

        /* 双引号字符串 */
        if (in_string == 1) {
            if (escape_state) {
                escape_string_char(out, c);
                escape_state = false;
            } else if (c == '\\') {
                escape_string_char(out, c);
                escape_state = true;
            } else if (c == '"') {
                escape_string_char(out, c);
                buf_append_str(out, "}/ETEX");
                in_string = 0;
            } else {
                escape_string_char(out, c);
            }
            i++;
            continue;
        }

        /* 单引号字符 */
        if (in_string == 2) {
            if (escape_state) {
                escape_string_char(out, c);
                escape_state = false;
            } else if (c == '\\') {
                escape_string_char(out, c);
                escape_state = true;
            } else if (c == '\'') {
                escape_string_char(out, c);
                buf_append_str(out, "}/ETEX");
                in_string = 0;
            } else {
                escape_string_char(out, c);
            }
            i++;
            continue;
        }

        /* 普通代码 */
        if (c == '/') {
            if (i+1 < len) {
                char next = line[i+1];
                if (next == '/') {
                    buf_append_str(out, "/BTEX\\color[" COMMENT_COLOR "]{");
                    escape_comment_char(out, '/');
                    escape_comment_char(out, '/');
                    i += 2;
                    while (i < len) {
                        escape_comment_char(out, line[i]);
                        i++;
                    }
                    buf_append_str(out, "}/ETEX");
                    break; /* 行尾 */
                } else if (next == '*') {
                    in_multi = true;
                    buf_append_str(out, "/BTEX\\color[" COMMENT_COLOR "]{");
                    escape_comment_char(out, '/');
                    escape_comment_char(out, '*');
                    i += 2;
                    continue;
                } else {
                    buf_append_char(out, c);
                }
            } else {
                buf_append_char(out, c);
            }
        } else if (c == '"') {
            in_string = 1;
            buf_append_str(out, "/BTEX\\color[" STRING_COLOR "]{");
            escape_string_char(out, c);
        } else if (c == '\'') {
            in_string = 2;
            buf_append_str(out, "/BTEX\\color[" STRING_COLOR "]{");
            escape_string_char(out, c);
        } else if (is_ident_start(c)) {
            size_t start = i;
            while (i < len && is_ident_cont(line[i])) i++;
            size_t tok_len = i - start;
            i--; /* 回退，循环会自增 */

            /* 向前看 '(' */
            size_t j = i + 1;
            while (j < len && (line[j] == ' ' || line[j] == '\t')) j++;
            if (j < len && line[j] == '(' && !is_keyword(line + start, tok_len)) {
                buf_append_str(out, "/BTEX\\color[" FUNC_COLOR "]{");
                escape_light(out, line + start, tok_len);
                buf_append_str(out, "}/ETEX");
            } else if (is_keyword(line + start, tok_len)) {
                buf_append_str(out, "/BTEX\\color[" KEYWORD_COLOR "]{");
                escape_light(out, line + start, tok_len);
                buf_append_str(out, "}/ETEX");
            } else {
                buf_append(out, line + start, tok_len);
            }
        } else if (is_digit(c)) {
            size_t start = i;
            while (i < len && is_num_part(line[i])) i++;
            size_t tok_len = i - start;
            i--;
            buf_append_str(out, "/BTEX\\color[" NUMBER_COLOR "]{");
            escape_light(out, line + start, tok_len);
            buf_append_str(out, "}/ETEX");
        } else {
            buf_append_char(out, c);
        }
        i++;
    }

    if (in_multi)
        buf_append_str(out, "}/ETEX");
    if (in_string == 1 || in_string == 2)
        buf_append_str(out, "}/ETEX");
}

/* ---------- 辅助检测函数 ---------- */
static bool is_single_at(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '@') return false;
    line++;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '\0';
}

static bool is_reference_line(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '#') return false;
    const char *at = strrchr(line, '@');
    if (!at) return false;
    /* 确保 @ 之后只有空白 */
    const char *p = at + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\0') return false;
    /* 确保 # 和 @ 之间没有多余的 @ */
    const char *first_at = strchr(line + 1, '@'); /* 跳过 # */
    if (first_at != at) return false;
    return true;
}

/* ---------- 主处理 ---------- */
static void process_line(char *line) {
    /* 去除行尾换行 */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') line[--len] = '\0';

    /* 跨行块头 */
    if (in_header) {
        if (strchr(line, '#')) {
            add_to_header(line);
            flush_header();
            in_code = true;
        } else {
            add_to_header(line);
        }
        return;
    }

    /* 代码块结束 */
    if (in_code && is_single_at(line)) {
        in_code = false;
        puts(line);
        return;
    }

    if (in_code) {
        if (is_reference_line(line)) {
            puts(line);
            return;
        }
        /* 核心：使用缓冲区高亮并输出 */
        Buf out;
        buf_init(&out);
        highlight_line(line, len, &out);
        fwrite(out.data, 1, out.len, stdout);
        putchar('\n');
        buf_free(&out);
        return;
    }

    /* 块开始 */
    if (line[0] == '@') {
        if (strchr(line, '#')) {
            puts(line);
            in_code = true;
        } else {
            in_header = true;
            add_to_header(line);
        }
        return;
    }

    /* 普通行 */
    puts(line);
}

int main(void) {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t n;

    while ((n = getline(&line, &bufsize, stdin)) != -1) {
        process_line(line);
    }

    if (in_header) flush_header();
    free(line);
    return 0;
}
