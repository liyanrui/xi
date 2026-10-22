\environment xi-style
\starttext
\title{Xi 的设计与实现}
\startalignment[center]
\blank[3*medium]
  {\tfa 李延瑞 (lyr.m2@live.cn)}
\blank[2*medium]
  {\tfa 2026 年 7 月}
\blank[3*medium]
\stopalignment

\preface{前言}

一个程序，其源码可以拆解为一组片段，这些片段如同一张又一张设计图，跟随作者的思路，分散在文字构成的情节之中。这种编程方式便是文学编程，由 Donald Knuth 于 20 世纪 80 年代提出。

文学程序通常是某种排版语言和编程语言的混合体。Knuth 以他发明的 \TEX\ 语言编写文档，以 Pascal 语言编写程序源码片段，且 \TEX\ 排版系统本身便是由这种方式实现的。Knuth 将生成可以排版印刷的程序文档的过程称为编织（Weave），将提取程序源码的过程称为抽取（Tangle），这两个过程所用的程序分别是 weave 和 tangle，二者合称 WEB。

Knuth 用 WEB 这个名字的时候，世上还没有互联网。他之所以用这个名字，大概是因为一篇文章或一本书是以网状结构描述事物，例如小说，各种人物错综复杂的关系可构成网络。WEB 只支持 \TEX\ 排版语言和 Pascal 编程语言。后来，Knuth 与 Silvio Levy 合作开发的 CWEB，能够支持基于 \TEX\ 语言和 C 编程语言的文学编程。为了扩大排版语言和编程语言的支持范围，Norman Ramsey 写出了 noweb，默认能够支持 \TEX，\LATEX\ 和 HTML 等排版语言，而编程语言则不受限制。不过，若要 noweb 支持其他排版语言，需要编写相应的后端程序，对于一般用户而言并非易事。

2017 年，我写了一个简单的文学编程工具 orez。该工具于 2024 年经过一次全面的重写，除了增加用户自定义文学编程标记的功能之外，变化主要发生在文档部分。事实上，即使代码完全不变，文学程序的修改空间也是无限的。如物理学家薛定谔所言，我们的任务不是去发现一些别人还没有发现的东西，而是针对所有人都看见的东西做一些从未有过的思考。对于代码的新的思考，总是可以将其记录在文学程序的文档部分。

\useURL[libwk][https://github.com/liyanrui/wk]
我的需求，orez 完全能满足，但它依赖 GLib 和 LibYAML 这两个大库的少量功能。今年，由于我穷得只剩下了时间，为了让自己不至于堕落，便先写了一个很小的 C 库，即 WK 库\footnote{\from[libwk]}，使之覆盖 orez 的需求，用以取代 GLib 和 LibYAML 库。然后我再度重写了 orez，并将其更名为 xi。此名在汉语里可对应「兮」，表示该工具简单到只是清淡的语气，它几乎不依赖任何东西。

\section[xi-mark]{标记}

\subsection{基本符号}

文学程序通常是由文档片段和源码片段组合而成，不妨将二者的关系想象为 DNA 的双链结构，彼此相连，相互缠绕。文档片段没有名字，后文将其称为无名片段。源码片段有名字且能可通过名字引用其内容，后文将其称为有名片段。xi 默认使用符号 \type{@} 标记两个相邻片段的界限，例如

\starttyping
计算 x 与 y 的点积：
ESC_AT
sum = 0;
for (i = 0; i < n; i++) sum += x[i] * y[i];
\stoptyping

符号 \type{@} 仅能帮助 xi 程序识别两个片段，却不能识别二者是无名片段还是有名片段。为此，xi 默认以符号 \type{#} 标记片段名字的终止，从而区分片段类型，例如

\starttyping[escape=yes]
ESC_AT 计算 x 与 y 的点积 ESC_SHARP
sum = 0;
for (i = 0; i < n; i++) sum += x[i] * y[i];
\stoptyping

\noindent 是有名片段，其名为「计算 x 与 y 的点积」。

片段的名字可能会过长，需要用续行符予以分行。xi 默认的续行符为反斜线 \type{\}，例如

\starttyping
ESC_AT 计算 x 与 y 的点积，\
  将所得结果存于变量 sum ESC_SHARP
sum = 0;
for (i = 0; i < n; i++) sum += x[i] * y[i];
\stoptyping

有名片段可被引用。若在一个有名片段中引用另一个有名片段，只需颠倒后者名字前后的「\type{@}」和「\type{#}」的位置便可构成对该片段的引用，例如

\starttyping
ESC_AT 点积 ESC_SHARP
int dot_product(double x[], double y[], int n) {
        int i;
        double sum;
        ESC_SHARP 计算 x 与 y 的点积 ESC_AT;
        return sum;
}
\stoptyping

\noindent 形如「\type{# ... @}」的文本结构称为有名片段的引用，此时 \type{@} 构成片段引用终结符，其后的同一行文本会被 xi 忽略，上例中位于引用终结符之后的分号以及注释文本「\type{/* 有名片段引用 */}」皆会被忽略。

在实际使用 xi 编写文学程序时，可以用 \type{@} 作为有名片段的终结符使用，例如

\starttyping
ESC_AT 点积 ESC_SHARP
int dot_product(double x[], double y[], int n) {
        int i;
        double sum;
        ESC_SHARP 计算 x 与 y 的点积 ESC_AT; /* 有名片段引用 */
        return sum;
}
ESC_AT
\stoptyping

\noindent 此举并不违背片段界限符规则。

\subsection{有名片段累加符}

有名片段可以同名。同名片段意味着一段复杂的程序源码被打碎，按照易于阅读和理解的原则散布于各处。xi 抽取程序源码时，会依序合并这些同名片段。例如，假设此处存在名为 foobar 的有名片段：

\starttyping
ESC_AT foobar ESC_SHARP
void foobar(void) {
        printf("foo ");
\stoptyping

\noindent 下面是同样名为「foobar」的有名片段，但是用了运算符 \type{+}，表示其内容是上一个同名片段的延续。

\starttyping
ESC_AT foobar ESC_SHARP +
        printf("bar\n");
}
\stoptyping

\noindent 这两个有名片段可被 xi 合并为以下代码：

\starttyping
void foobar(void) {
        printf("foo ");
        printf("bar\n");
}
\stoptyping

运算符 \type{+} 意味着当前有名片段会被追加到上一个同名片段的尾部。如果我们想将一个有名片段在累加时放在另一个有名片段之前，还需要引入一个运算符用于修饰 \type{+}，表示向前追加，xi 默认用 \type{^} 作为该修饰符\footnote{在标准键盘上，\type{^} 与数字 6 位于同一键位。}，例如

\starttyping
ESC_AT foobar ESC_SHARP
        printf("bar\n");

ESC_AT foobar ESC_SHARP ^+
void foobar(void) {
        printf("foo ");
\stoptyping

\noindent 对于上述两个同名片段，xi 的合并结果为

\starttyping
void foobar(void) {
        printf("foo ");
        printf("bar\n");
}
\stoptyping

\subsection{标签}

若将一个有名片段累加到指定的同名片段的之前或之后，必须为后者提供一个特指标记，该标记称为标签。例如

\starttyping
ESC_AT foobar ESC_SHARP
        printf("bar\n");
}

ESC_AT foobar ESC_SHARP ^+
<tag>
void foobar(void) {
        printf("foo ");

ESC_AT foobar ESC_SHARP <lable> ^+
#include <stdio.h>
\stoptyping

\noindent xi 对上述三个有名片段的合并结果为

\starttyping
#include <stdio.h>
void foobar(void) {
        printf("foo ");
        printf("bar\n");
}
\stoptyping

\noindent 上述示例中，第二个有名片段首部之后的 \type{<tag>} 便是该有名片段的标签。第三个有名片段首部之后的运算符 \type{^+} 之前的 \type{<tag>}，表示当前的有名片段需要合并至带有该标签的同名片段之前。注意，有名片段的标签需要具备唯一性，亦即 xi 不允许两个同名片段使用相同标签。

\subsection[lang-mark]{编程语言}

尽管 xi 所支持的文学编程独立于任何编程语言，但是在文档排版过程中为了实现有名片段的高亮渲染，需要知道有名片段所用的编程语言，可在有名片段的首部的特定位置放置一个标记表示该片段所用编程语言，例如

\starttyping
ESC_AT foobar ESC_SHARP [C]
void foobar(void) {
        puts("foobar");
}
\stoptyping

\noindent 标记 \type{[C]} 表示有名片段所用的编程语言为 C 语言。

编程语言标记位于有名片段首部之后，可在引用标签之前，也可在其之后，但必须位于累加运算符之前。

每一个有名片段都需要编程语言标记吗？当然不是。假设一个带有语言标记的有名片段 $X$，xi 能够将该语言标记自动传播给 1) 与 $X$ 同名的有名片段，2) 被 $X$ 引用的有名片段，3) 引用 $X$ 的有名片段。语言标记是可选的。遇到为提供语言标记的有名片段，xi 会给出警告。

\section{词法}

我并未学过编译原理——曾经想学，却未能看懂教科书——，对其认知也颇为粗浅，故而我并不能真正在学术上确定本节内容是否属于编译原理中的词法分析阶段，只是在情感上我觉得它应该是。为了降低解析难度，在定义了记号结构以及妥善处置配置文件之后，我将词法分析过程分为五个阶段，每个阶段只处理少量的记号。

\subsection{记号}

在第 \in[xi-mark] 节中，已给出 xi 程序所用的所有默认符号及其含义。除这些符号之外，xi 将文学程序里的其他内容视为普通文本。在词法分析过程中，无论是符号还是普通文本，其记号表示为

@ 记号 # [C]
typedef struct {
        XITokenType type;
        size_t line_number;
        WKStr *content;
} XIToken;
@

\noindent 其中 \type{type} 字段表示记号类型，共有以下类型

@ 记号 # ^+
typedef enum {
        XI_SNIPPET_DELIMITER, /* 文档或有名片段界限符 */
        XI_SNIPPET_NAME_DELIMITER, /* 片段名字界限符 */
        XI_LANGUAGE_START_MARK, /* 语言标记开始符 */
        XI_LANGUAGE_STOP_MARK, /* 语言标记终结符 */
        XI_SNIPPET_APPENDING_MARK, /* 后向合并运算符 */
        XI_SNIPPET_PREPENDING_MARK, /*前向合并运算符 */
        XI_TAG_START_MARK, /* 有名片段标签开始符 */
        XI_TAG_STOP_MARK, /* 有名片段标签终结符 */
        XI_SNIPPET_REFERENCE_START_MARK, /* 有名片段引用开始符 */
        XI_SNIPPET_REFERENCE_STOP_MARK, /* 有名片段引用终结符 */
        XI_TEXT /* 普通文本 */
} XITokenType;
@

\noindent \type{line_number} 和 \type{content} 字段分别用于记录记号起始行号和内容。

第 \in[xi-mark] 节中涉及的符号，皆为 xi 默认符号。xi 允许用户通过配置文件自定义符号，用以代替这些默认的符号。为了达成这一目标，需要为符号定义一个变量集：

@ 记号 # +
typedef struct {
        WKStr *snippet_delimiter;
        WKStr *snippet_name_delimiter;
        WKStr *snippet_name_continuation;
        WKStr *language_start_mark;
        WKStr *language_stop_mark;
        WKStr *snippet_appending_mark;
        WKStr *snippet_prepending_mark;
        WKStr *tag_start_mark;
        WKStr *tag_stop_mark;
        WKStr *snippet_reference_start_mark;
        WKStr *snippet_reference_stop_mark;
} XISymbols;
XISymbols *xi_symbols;
@

\noindent 全局变量 \type{xi_symbols} 表示 xi的符号集，需要将其初始化为 xi 默认符号：

@ 符号表初始化 # [C]
xi_symbols = malloc(sizeof(XISymbols));
*xi_symbols = (XISymbols){
        .snippet_delimiter = wk_str("@"),
        .snippet_name_delimiter = wk_str("#"),
        .snippet_name_continuation = wk_str("\\"),
        .language_start_mark = wk_str("["),
        .language_stop_mark = wk_str("]"),
        .snippet_appending_mark = wk_str("+"),
        .snippet_prepending_mark = wk_str("^+"),
        .tag_start_mark = wk_str("<"),
        .tag_stop_mark = wk_str(">"),
        .snippet_reference_start_mark = wk_str("#"),
        .snippet_reference_stop_mark = wk_str("@")
};
@

\noindent 若用户未通过 xi 配置文件提供自定义符号，则词法分析过程会默认使用 \type{xi_symbols} 的初始值作为 xi 文学编程符号。

xi 的配置文件，其形式上是一组键值对，键名与 \type{XIConfig} 的成员变量名相同，例如

\starttyping
snippet_reference_start_mark: 「
snippet_reference_stop_mark: 」
\stoptyping

在解析 xi 的配置文件时，需要借助 WK 库的 WKCfg 模块。我将 xi 外部库的头文件包含代码统一放在名为「外景」的代码片段内，

@ 外境 # [C]
#include <wk-cfg.h>
@

\noindent WKCfg 模块依赖 WKStr（字符串模块）、WKArray（动态数组模块）、WKList（双向链表模块）以及 WKTable（散列表模块），后面这些模块也皆为 xi 所需，后者的头文件皆已被 wi-cfg.h 包含。

假设配置文件路径变量为 \type{xi_config_path}，配置文件的载入及解析过程如下：

@ 配置文件解析 #
WKTable *user_config = xi_config_path ? wk_cfg(xi_config_path) : NULL;
/* 用 user_config 中的设定，替换 xi_config 中的字段 */
if (user_config) {
        XI_USER_CONFIG(user_config, xi_symbols, snippet_delimiter);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_name_delimiter);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_name_continuation);
        XI_USER_CONFIG(user_config, xi_symbols, language_start_mark);
        XI_USER_CONFIG(user_config, xi_symbols, language_stop_mark);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_appending_mark);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_prepending_mark);
        XI_USER_CONFIG(user_config, xi_symbols, tag_start_mark);
        XI_USER_CONFIG(user_config, xi_symbols, tag_stop_mark);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_reference_start_mark);
        XI_USER_CONFIG(user_config, xi_symbols, snippet_reference_stop_mark);
}
@

\noindent 配置文件的解析结果 \type{user_config} 是一个散列表。上述代码所用的宏 \type{XI_USER_CONFIG} 可从 \type{user_config} 中查询相应的符号，并予以替换：

@ 记号 # +
#define XI_USER_CONFIG(user_config, xi_symbols, item) do { \
        WKBox *v = wk_table_query(user_config, wk_box_ref(#item, const char *)); \
	if (v) { \
                if (xi_symbols->item) wk_str_free(xi_symbols->item); \
                xi_symbols->item = wk_str(wk_box_get(v, WKStr *)->body); \
        } \
} while (0)
@

\noindent 上述宏定义里，\type{#item} 表示给 \type{item} 的值添加双引号，使之成为 C 字符串常量。

在 xi 进程退出前，配合文件解析结果 \type{user_config} 以及符号集 \type{xi_symbols} 占用的内存由以下代码释放：

@ xi 进程资源回收 #
if (user_config) wk_table_free(user_config);
wk_str_free(xi_symbols->snippet_delimiter);
wk_str_free(xi_symbols->snippet_name_delimiter);
wk_str_free(xi_symbols->snippet_name_continuation);
wk_str_free(xi_symbols->language_start_mark);
wk_str_free(xi_symbols->language_stop_mark);
wk_str_free(xi_symbols->snippet_appending_mark);
wk_str_free(xi_symbols->snippet_prepending_mark);
wk_str_free(xi_symbols->tag_start_mark);
wk_str_free(xi_symbols->tag_stop_mark);
wk_str_free(xi_symbols->snippet_reference_start_mark);
wk_str_free(xi_symbols->snippet_reference_stop_mark);
free(xi_symbols);
@

\subsection{输入}

假设文学程序文件路径为 \type{xi_file_path}，打开该文件，

@ 打开文学程序文件 -> xi_file # [C]
FILE *xi_file = xi_file_path ? fopen(xi_file_path, "r") : stdin;
if (!xi_file) {
	fprintf(stderr, "Failed to open %s\n", xi_file_path);
	exit(EXIT_FAILURE);
}
@

在 xi 进程退出前，关闭文件：

@ xi 进程资源回收 # +
fclose(xi_file);
@

在读取文学程序文件的过程中，需要一个缓冲区临时存储读入的字符。辅助函数 \type{read_char} 可从文件中读取当前字符（一个字节），将其存入缓冲区，

@ 词法分析阶段的辅助函数 # [C]
static int read_char(FILE *xi_file, WKStr *cache) {
        int c = fgetc(xi_file);
        if (c != EOF) {
                wk_str_suffix_char(cache, (char)c);
                return c;
        } else return EOF;
}
@

\subsection{分段}

词法分析第一阶段的主要任务是将文学程序文件读入内存，基于无名片段和有名片段的界限符，将两种片段分割开来，从而形成初始的记号列表——由无名片段、有名片段以及界限符对应的记号构成。

由于用户可以配置自己喜欢的片段界限符，导致 xi 无法假设界限符的宽度（占用的字节数），故而从文学程序文件中每读取一个字节，皆需需检测缓冲区的尾端是否符合「换行符 + 空白字符 + \type{XI_SNIPPET_DELIMITER}」模式，亦即无名片段与有名片段的分割符必须位于一行的开头，其前只允许出现空白字符（空格、全角空格或制表符）。

@ 词法分析阶段的辅助函数 # +
static bool tail_is_snippet_delimiter(WKStr *cache) {
        WKStr *snippet_delimiter = xi_symbols->snippet_delimiter;
        if (snippet_delimiter->n > cache->n) return false;
        const char *p = cache->body + cache->n - snippet_delimiter->n;
        if (strcmp(p, snippet_delimiter->body) != 0) return false;
        if (p == cache->body) return true;
        /* 构造字符串区间 [a, b)，若该区间存在不可忽略的字符，结果为 false，否则为 true */
        const char *a = cache->body;
        const char *b = p;
        p = xi_skip(a, b, p, -1, xi_skipped_chars);
        return (p == a || *(p - 1) == '\n') ? true : false;
}
@

\noindent \type{xi_skip} 可以在字符串区间 \type{[a, b)} 中移动 \type{c}，令其根据方向 \type{dir} 跳过所有属于 \type{xi_skipped_chars} 的字符，并返回其终值。\type{dir} 小于 0 表示向 \type{a} 的方向移动，否则向 \type{b} 的方向移动，倘若无法移动，则返回 \type{c} 的原值。

@ 词法分析阶段的辅助函数 # ^+
static const char *xi_skip(const char *a, const char *b,
                           const char *pos, int dir, WKArray *skipped_chars) {
        /* pos 不在 [a, b) 区间，其值不变 */
	if (pos <= a && dir < 0) return pos;
	if (pos >= b && dir >= 0) return pos;
        /* 从 pos 当前位置跨越 skipped_chars 中的字符组合成的一段空白区，记录新位置 */
        while (1) {
		const char *u = pos;
                for (size_t i = 0; i < skipped_chars->n; i++) {
                        WKStr *s = wk_array_get(skipped_chars, i, WKStr *);
                        const char *c = dir < 0 ? pos - s->n : pos;
                        if (c < a || c + s->n > b) continue;
			if (memcmp(s->body, c, s->n) == 0) {
				pos = dir < 0 ? c : c + s->n;
			}
                }
                if (u == pos) break;
        }
        return pos;
}
@

全局变量 \type{xi_skipped_chars} 存储了可用于 \type{xi_skip} 函数的空白字符集，其定义、初始化和回收过程为

@ 全局变量 #
WKArray *xi_skipped_chars; /* xi 空白字符集 */
@

@ 准备可忽略的空白字符集 #
xi_skipped_chars = wk_array(WKStr *);
wk_array_add(xi_skipped_chars, wk_str(" "), WKStr *);
wk_array_add(xi_skipped_chars, wk_str("\t"), WKStr *);
wk_array_add(xi_skipped_chars, wk_str("　"), WKStr *);
@

@ xi 进程资源回收 # +
for (size_t i = 0; i < xi_skipped_chars->n; i++) {
        WKStr *s = wk_array_get(xi_skipped_chars, i, WKStr *);
        wk_str_free(s);
}
wk_array_free(xi_skipped_chars);
@

若 \type{tail_is_snippet_delimiter} 返回 \type{true}，表明缓冲区尾部是片段界限符，此时该符号完成了它的使命，将其从缓冲区中移除，

@ 移除缓冲区尾部的片段界限符 #
size_t sni_del_length = xi_symbols->snippet_delimiter->n;
size_t sni_del_begin = cache->n - sni_del_length;
wk_str_del(cache, sni_del_begin, sni_del_length);
@

\noindent 删除缓冲区尾部的片段界限符后，剩余字符作为片段内容，记为类型 \type{XI_TEXT}，存于记号列表：

@ 词法分析阶段的辅助函数 # +
static void add_snippet(WKList *tokens, WKStr *cache) {
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_TEXT;
        # 获取当前记号对应的行号 n @
        snippet->line_number = n;
        snippet->content = cache;
        wk_list_suffix(tokens, snippet, XIToken *);
}
@

\noindent 基于上一个记号的行号以及该记号内容的行数确定新记号的行号：

@ 获取当前记号对应的行号 n #
size_t n;
WKLink *tail = tokens->tail;
if (tail) {
        XIToken *last_token = wk_link_get(tail, XIToken *);
        n = last_token->line_number;
        for (const char *p = last_token->content->body; *p != '\0'; p++) {
                if (*p == '\n') n++;
        }
} else  n = 1;
@

在分段过程中，每次向记号列表中插入类型为 \type{XI_TEXT} 的记号后，需要向记号列表插入片段界限符：

@ 词法分析阶段的辅助函数 # +
static void add_snippet_delimiter(WKList *tokens) {
        WKStr *snippet_delimiter = xi_symbols->snippet_delimiter;
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_SNIPPET_DELIMITER;
        # 获取当前记号对应的行号 n @
        snippet->line_number = n;
        snippet->content = wk_str(snippet_delimiter->body);
        wk_list_suffix(tokens, snippet, XIToken *);
}
@

对文学程序文件 \type{xi_file} 分段，将结果保存于记号类表 \type{xi_tokens}，该过程如下：

@ 分段 # [C]
WKList *xi_tokens = wk_list(XIToken *);
WKStr *cache = wk_str(NULL);
while (1) {
        int status = read_char(xi_file, cache);
        if (status == EOF) break;
        else {
                bool t = tail_is_snippet_delimiter(cache);
                if (t) {
                        # 移除缓冲区尾部的片段界限符 @
                        add_snippet(xi_tokens, cache);
                        add_snippet_delimiter(xi_tokens);
                        cache = wk_str(NULL); /* 刷新 cache */
                }
        }
}
if (cache->n > 0) {
        add_snippet(xi_tokens, cache);
        /* 以片段界限符作为记号列表的结尾，便于在语法树的构造过程中析取片段内容 */
        add_snippet_delimiter(xi_tokens);
} else wk_str_free(cache);
@

\subsection{片段名}

词法分析的第二阶段仅处理记号列表 \type{xi_tokens} 中类型为 \type{XI_TEXT} 的节点。任务依然简单，只需基于片段名字界限符，将每个含有该符号的文本节点拆分为三个记号，其类型分别为 \type{XI_TEXT}，\type{XI_SNIPPET_NAME_DELIMITER} 和 \type{XI_TEXT}。

首先，定义辅助函数 \type{find_name_delimiter}，使之能在记号内容中搜索片段名字界限符的位置。该函数在遍历节点内容的过程中，若遇到与片段名字界限符相同的字符，则回溯前文，确认是否出现换行符。只有在前文未出现换行符的情况下，方可将该字符判定为片段名字界限符，返回其位置。在回溯检测前文过程中，需要排查是否存在续行符引导的换行情况，将符合该情况的换行符视为普通字符，使其不影响片段名字界限符的判定。

@ 词法分析阶段的辅助函数 # +
static const char *find_name_delimiter(XIToken *t) {
        WKStr *name_delimiter = xi_symbols->snippet_name_delimiter;
        WKStr *continuation = xi_symbols->snippet_name_continuation;
        /* 构造字符串区间 [a, b) */
        const char *a = t->content->body;
        if (*a == '\n') return NULL;
        /* b 为 NULL 或指向片段名字界限符首字节 */
        const char *b = strstr(a, name_delimiter->body);
        if (!b || b == a) return NULL;
        /* 检测 [a, b) 是否含有合法的片段名字界限符 */
        const char *p = b;
        enum {IDLE, LINEBREAK, SUCCESS, FAILURE} state = IDLE;
        while (1) { /* 逆序遍历 [a, b) */
                p = xi_skip(a, b, p, -1, xi_skipped_chars);
                if (p == a) {
                        state = SUCCESS;
                        break;
                } else p--;
                switch (state) {
                case IDLE:
                        if (*p == '\n') state = LINEBREAK;
                        break;
                case LINEBREAK:
                        if (here_is_me(a, b, p, -1, continuation)) {
				p -= (continuation->n - 1);
                                state = IDLE;
                        } else state = FAILURE;
                        break;
                default:
			fprintf(stderr, "Illegal state in line %lu\n", t->line_number);
                }
                if (state == FAILURE || state == SUCCESS) break;
        }
        return (state == SUCCESS) ? b : NULL;
}
@

\noindent 上述代码所用的辅助函数 \type{here_is_me} 用于在区间 \type{[a, b)} 内判断以 \type{here} 为起始位置的一段子串是否与 \type{me} 相同。\type{dir} 表示 \type{here} 的取向，若 \type{dir} 为负值，则子串是 \type{[here - n + 1, here]}，否则是 \type{[here, here + n - 1]}，\type{n} 为 \type{me} 的长度。

@ 词法分析阶段的辅助函数 # ^+
static bool here_is_me(const char *a, const char *b,
                       const char *here, int dir, WKStr *me) {
        size_t n = me->n;
        const char *p = dir < 0 ? (here - n + 1) : here;
        if (p < a) return false;
        if (p + n > b) return false;
        return memcmp(me->body, p, n) == 0 ? true : false;
}
@

若 \type{find_name_delimiter} 发现符合规则的片段名字界限符则返回界限符首地址，否则返回 \type{NULL}。需要注意的是，若界限符之前皆为空白字符，\type{find_name_delimiter} 依然会认定为界限符合乎规则，即片段名字为空白字串。

遍历现有的记号列表，将其中对含有片段名字界限符的记号分割为三个记号，并以后者取代前者：

@ 析取片段名界限符 #
WKLink *it = xi_tokens->head;
while (it) {
        XIToken *t = wk_link_get(it, XIToken *);
        if (t->type == XI_TEXT) {
                const char *p = find_name_delimiter(t);
                if (p) {
                        # 基于 p 分割 t，得到 snippet_name, \
                          snippet_name_delimiter 和 snippet @
                        /* 删除 it */
                        WKLink *next = it->next;
			wk_str_free(t->content);
                        free(t);
			wk_list_del(xi_tokens, it);
                        if (next) { /* 在 next 之前插入新节点 */
			        WK_LIST_INSF(xi_tokens, next, &snippet_name);
                                WK_LIST_INSF(xi_tokens, next, &snippet_name_delimiter);
                                WK_LIST_INSF(xi_tokens, next, &snippet);
                                it = next->prev; /* 调整迭代指针 */
                        } else { /* 在记号列表尾部插入新节点 */
                                wk_list_suffix(xi_tokens, snippet_name, XIToken *);
                                wk_list_suffix(xi_tokens, snippet_name_delimiter, XIToken *);
                                wk_list_suffix(xi_tokens, snippet, XIToken *);
                                it = xi_tokens->tail; /* 调整迭代指针 */
                        }
                }
        }
        it = it->next;
}
@

\noindent 在上述代码中，基于 \type{p} 分割记号 \type{t} 的过程如下：

@ 基于 p 分割 t，得到 snippet_name, \
  snippet_name_delimiter 和 snippet #
/* 构建片段名字记号 */
XIToken *snippet_name = malloc(sizeof(XIToken));
snippet_name->type = XI_TEXT;
snippet_name->line_number = t->line_number;
cache = wk_str(NULL);
for (const char *q = t->content->body; q != p; q++) {
        wk_str_suffix_char(cache, *q);
}
snippet_name->content = cache;
/* 构建片段名字界限符记号 */
XIToken *snippet_name_delimiter = malloc(sizeof(XIToken));
snippet_name_delimiter->type = XI_SNIPPET_NAME_DELIMITER;
snippet_name_delimiter->line_number = snippet_name->line_number;
for (const char *q = snippet_name->content->body; *q != '\0'; q++) {
        if (*q == '\n') snippet_name_delimiter->line_number++;
}
snippet_name_delimiter->content = wk_str(xi_symbols->snippet_name_delimiter->body);
/* 构建片段内容记号 */
XIToken *snippet = malloc(sizeof(XIToken));
snippet->type = XI_TEXT;
snippet->line_number = snippet_name_delimiter->line_number;
cache = wk_str(NULL);
for (const char *q = p + xi_symbols->snippet_name_delimiter->n; *q != '\0'; q++) {
        wk_str_suffix_char(cache, *q);
}
snippet->content = cache;
@

\subsection{语言标记}

假设记号 \type{t}，其类型为 \type{XI_TEXT} 且其前一个记号类型为 \type{XI_SNIPPET_NAME_DELIMITER}，则该记号内容的首部可能存在编程语言标记、标签以及标签引用，针对它们的析取过程皆依赖于以下辅助函数：

@ 词法分析阶段的辅助函数 # +
<extract_block>
static WKStr *extract_block(XIToken *t, WKStr *start_mark, WKStr *stop_mark) {
        WKStr *result = NULL;
        WKPair *block = find_block(t->content, start_mark, stop_mark);
        if (block) { /* 从记号内容里消除块标记 */
                const char *start = wk_box_get(block->x, const char *);
                const char *stop = wk_box_get(block->y, const char *);
                /* 更新行号 */
                size_t new_line_number = t->line_number;
                for (const char *p = start; p != stop; p++) {
                        if (*p == '\n') new_line_number++;
                }
                /* 获取块之后的内容 */
                WKStr *new_content = wk_str(NULL);
                for (const char *p = stop + stop_mark->n; *p != '\0'; p++) {
                        wk_str_suffix_char(new_content, *p);
                }
                /* 获取块内容 */
                result = get_block(t->content, block);
                /* 更新 t */
                wk_str_free(t->content);
                t->line_number = new_line_number;
                t->content = new_content;
        }
        return result;
}
@

\noindent \type{extract_block} 返回记号 \type{t} 首部的块标记的内容部分，若 \type{t} 的首部未出现块标记，则返回 \type{NULL}。上述代码所用的辅助函数 \type{find_block} 定义如下：

@ 词法分析阶段的辅助函数 # <extract_block> ^+
static WKPair *find_block(WKStr *content, WKStr *start_mark, WKStr *stop_mark) {
        WKPair *result = NULL;
        const char *block_start = NULL;
        const char *block_stop = NULL;
        const char *a = content->body;
        const char *b = a + content->n;
        const char *p = a;
        size_t m = start_mark->n;
        size_t n = stop_mark->n;
        enum {IDLE, MAYBE_MARK, FAILURE, SUCCESS} state = IDLE;
        while (1) { /* 正序遍历 [a, b - 1] */
                p = xi_skip(a, b, p, 1, xi_skipped_chars);
                switch (state) {
                case IDLE:
                        if (here_is_me(a, b, p, 1, start_mark)) {
                                block_start = p;
                                p += (m - 1);
                                state = MAYBE_MARK;
                        } else if (*p == '\n') ;
                        else state = FAILURE;
                        break;
                case MAYBE_MARK:
                        if (here_is_me(a, b, p, 1, stop_mark)) {
                                p += (n - 1);
                                block_stop = p + 1;
                                state = SUCCESS;
                        } else state = MAYBE_MARK;
                        break;
                default: ;
                }
                if (state == SUCCESS) {
                        result = wk_pair(wk_box(block_start + m, const char *),
                                         wk_box(block_stop - n, const char *));
                        break;
                } else if (state == FAILURE || *p == '\0') break;
                else p++;
        }
        return result;
}
static WKStr *get_block(WKStr *content, WKPair *block) {
        WKStr *result = wk_str(NULL);
        const char *start = wk_box_get(block->x, const char *);
        const char *stop = wk_box_get(block->y, const char *);
        for (const char *p = start; p != stop; p++) {
                wk_str_suffix_char(result, *p);
        }
        wk_box_free(block->x);
        wk_box_free(block->y);
        free(block);
        return result;
}
@

设 \type{t} 在记号列表 \type{xi_tokens} 中对应的节点为 \type{x}，对于 \type{extract_block} 返回的有效结果，为其构建记号的过程由以下辅助函数实现：

@ 词法分析阶段的辅助函数 # +
static void add_block(WKList *tokens,
                      WKLink *x,
                      WKStr *block,
                      WKStr *start_mark,
                      XITokenType start_mark_type,
                      WKStr *stop_mark,
                      XITokenType stop_mark_type)
{
        XIToken *t = wk_link_get(x, XIToken *);
        /* 构建块起始记号 */
        XIToken *start = malloc(sizeof(XIToken));
        start->type = start_mark_type;
        start->line_number = t->line_number;
        start->content = wk_str(start_mark->body);
        WK_LIST_INSF(tokens, x, &start);
        /* 构建块内容记号 */
        XIToken *body = malloc(sizeof(XIToken));
        body->type = XI_TEXT;
        body->line_number = t->line_number;
        body->content = block;
        WK_LIST_INSF(tokens, x, &body);
        /* 构建块终止记号 */
        XIToken *stop = malloc(sizeof(XIToken));
        stop->type = stop_mark_type;
        stop->line_number = t->line_number;
        stop->content = wk_str(stop_mark->body);
        WK_LIST_INSF(tokens, x, &stop);
}
@

对于记号 \type{t} 及其所属记号节点 \type{it}，语言标记的提取与记号构建过程如下：

@ 语言标记提取与记号构建 #
WKStr *language = extract_block(t, xi_symbols->language_start_mark, xi_symbols->language_stop_mark);
if (language) {
        add_block(xi_tokens,
                  it,
                  language,
                  xi_symbols->language_start_mark,
                  XI_LANGUAGE_START_MARK,
                  xi_symbols->language_stop_mark,
                  XI_LANGUAGE_STOP_MARK);
}
@

假设 \type{xi_tokens} 的遍历指针为 \type{it}，则语言标记析取过程实现如下：

@ 析取语言标记 #
it = xi_tokens->head;
while (1) {
        if (!it) break;
        XIToken *t = wk_link_get(it, XIToken *);
        if (t->type == XI_TEXT) {
                WKLink *prev = it->prev;
                if (prev) {
                        XIToken *a = wk_link_get(prev, XIToken *);
                        if (a->type == XI_SNIPPET_NAME_DELIMITER) {
                                # 语言标记提取与记号构建 @
                        }
                }
        }
        it = it->next;
}
@

\subsection{标签引用与运算符}

若有名片段首部存在标签引用，则必定存在运算符，且标签引用须在运算符之前。若有名片段首部不存在标签引用，依然有可能存在运算符。标签引用部分的析取，可基于 \type{extract_block} 和 \type{add_block} 函数实现。

对于记号 \type{t} 及其所属记号节点 \type{it}，实现标签引用提取与记号构建过程如下：

@ 标签引用提取与记号构建 #
WKStr *tag_reference = extract_block(t, xi_symbols->tag_start_mark, xi_symbols->tag_stop_mark);
if (tag_reference) {
        add_block(xi_tokens,
                  it,
                  tag_reference,
                  xi_symbols->tag_start_mark,
                  XI_TAG_START_MARK,
                  xi_symbols->tag_stop_mark,
                  XI_TAG_STOP_MARK);
}
@

运算符需要专门的函数予以提取：

@ 词法分析阶段的辅助函数 # +
static WKStr *extract_operator(XIToken *t, WKStr *operator) {
        /* 构造字符串区间 [a, b) */
        const char *a = t->content->body;
        const char *b = strstr(a, operator->body);
        if (!b) return NULL;
        bool is_legal = true;
        if (b > a) {
                const char *p = b;
                p = xi_skip(a, b, p, -1, xi_skipped_chars);
                if (p != a) is_legal = false;
        }
        WKStr *result = NULL;
        if (is_legal) {
                /* 从 t 中删除片段追加运算符及其之前的空白字符 */
                WKStr *new_content = wk_str(NULL);
                size_t new_line_number = t->line_number;
                for (const char *p = a; p != b; p++) {
                        if (*p == '\n') new_line_number++;
                }
                for (const char *p = b + operator->n; *p != '\0'; p++) {
                        wk_str_suffix_char(new_content, *p);
                }
                wk_str_free(t->content);
                t->content = new_content;
                t->line_number = new_line_number;
                /* 构造片段追加运算符副本 */
                result = wk_str(operator->body);
        }
        return result;
}
@

设记号 \type{t} 在记号列表 \type{xi_tokens} 中所属结点为 \type{x}，以下函数向记号列表追加追加片段运算符记号：

@ 词法分析阶段的辅助函数 # +
static void add_operator(WKList *tokens,
                         WKLink *x,
                         WKStr *operator,
                         XITokenType operator_type) {
        XIToken *t = wk_link_get(x, XIToken *);
        XIToken *a = malloc(sizeof(XIToken));
        a->type = operator_type;
        a->line_number = t->line_number;
        a->content = operator;
        WK_LIST_INSF(tokens, x, &a);
}
@

对记号列表 \type{xi_tokens} 中的节点 \type{it} 所包含的记号 \type{t} 中的片段追加运算符的析取及其记号构建过程如下：

@ 片段追加运算符提取及记号构建 #
WKStr *appending_mark = extract_operator(t, xi_symbols->snippet_appending_mark);
if (appending_mark) {
        add_operator(xi_tokens,
                     it,
                     appending_mark,
                     XI_SNIPPET_APPENDING_MARK);
} else {
        WKStr *prepending_mark = extract_operator(t, xi_symbols->snippet_prepending_mark);
        if (prepending_mark) {
                add_operator(xi_tokens,
                             it,
                             prepending_mark,
                             XI_SNIPPET_PREPENDING_MARK);
        }
}
@


假设 \type{xi_tokens} 的遍历指针为 \type{it}，则标签引用和运算符的析取过程的实现如下：

@ 析取标签引用和运算符 #
it = xi_tokens->head;
while (1) {
        if (!it) break;
        XIToken *t = wk_link_get(it, XIToken *);
        if (t->type == XI_TEXT) {
                WKLink *prev = it->prev;
                if (prev) {
                        XIToken *a = wk_link_get(prev, XIToken *);
                        if (a->type == XI_SNIPPET_NAME_DELIMITER
                            || a->type == XI_LANGUAGE_STOP_MARK) {
                                # 标签引用提取与记号构建 @
                                # 片段追加运算符提取及记号构建 @
                        }
                }
        }
        it = it->next;
}
@

\subsection{标签}

在扫描过程的第四阶段，析取片段标签，过程与上一节中标签引用的析取过程相同。对记号列表 \type{xi_tokens} 中的节点 \type{it} 所包含的记号 \type{t} 中的标签提取及其记号构建过程如下：

@ 标签提取及记号构建 #
WKStr *tag_mark = extract_block(t, xi_symbols->tag_start_mark, xi_symbols->tag_stop_mark);
if (tag_mark) {
        add_block(xi_tokens,
                  it,
                  tag_mark,
                  xi_symbols->tag_start_mark,
                  XI_TAG_START_MARK,
                  xi_symbols->tag_stop_mark,
                  XI_TAG_STOP_MARK);
}
@

假设 \type{xi_tokens} 的遍历指针为 \type{it}，则标签的解析过程如下：

@ 析取标签 #
it = xi_tokens->head;
while (1) {
        if (!it) break;
        XIToken *t = wk_link_get(it, XIToken *);
        if (t->type == XI_TEXT) {
                WKLink *prev = it->prev;
                if (prev) {
                        XIToken *a = wk_link_get(prev, XIToken *);
                        if (a->type == XI_SNIPPET_NAME_DELIMITER
                            || a->type == XI_LANGUAGE_STOP_MARK
                            || a->type == XI_SNIPPET_APPENDING_MARK
                            || a->type == XI_SNIPPET_PREPENDING_MARK) {
                                # 标签提取及记号构建 @
                        }
                }
        }
        it = it->next;
}
@

\subsection{片段引用}

设记号 \type{t} 类型为 \type{XI_TEXT}，在记号列表中属于节点 \type{x}，若其前节点所含记号并非以下类型之一：

\startitemize
\item \type{XI_SNIPPET_DELIMITER}
\item \type{XI_LANGUAGE_START_MARK}
\item \type{XI_TAG_START_MARK}
\stopitemize

\noindent 则 \type{t} 中可能含有片段引用，需对其予以析取。

片段引用析取过程的关键之处在于从 \type{t} 的内容中逐一析取片段引用标记。辅助函数 \type{find_snippet_reference} 可返回 \type{t} 中第一个片段引用标记（假如它存在），其定义如下：

@ 词法分析阶段的辅助函数 # +
static WKPair *find_snippet_reference(WKStr *content)
{
        WKStr *reference_start_mark = xi_symbols->snippet_reference_start_mark;
        WKStr *reference_stop_mark = xi_symbols->snippet_reference_stop_mark;
        WKStr *continuation = xi_symbols->snippet_name_continuation;
        const char *a = content->body, *begin = NULL, *end = NULL;
        enum {IDLE, MAYBE_LINEBREAK, FAILURE} state;
        while (1) {
                begin = strstr(a, reference_start_mark->body);
                if (!begin) return NULL;
                end = strstr(begin + 1, reference_stop_mark->body);
                if (!end) return NULL;
                /*区间 [c, p) 是片段名 */
                const char *p = end;
                const char *c = begin + reference_start_mark->n;
                /* 逆序遍历 [c, end)，校验片段名是否合法 */
                bool legal = true;
                state = IDLE;
                while (1) {
                        p = xi_skip(c, end, p, -1, xi_skipped_chars);
                        if (p == c) break;
                        p--;
                        switch (state) {
                        case IDLE:
                                if (*p == '\n') state = MAYBE_LINEBREAK;
                                break;
                        case MAYBE_LINEBREAK:
                                if (here_is_me(c, end, p, -1, continuation)) {
                                        state = IDLE;
                                } else state = FAILURE;
                                break;
                        default:
                                fprintf(stderr, "Illegal state in <<< %s >>>.\n", a);
                        }
                        if (state == FAILURE) {
                                legal = false;
                                break;
                        }
                }
                if (legal) break;
                a = c;
        }
        if (begin && end) {
                WKPair *result = wk_pair(wk_box(begin, const char *),
                                         wk_box(end + reference_stop_mark->n, const char *));
                return result;
        } else return NULL;
}
@

在能够析取 \type{t}$\in$\type{x} 的内容中片段引用标记后，便可基于析取结果，将 \type{t} 的内容分割为五个部分——片段引用之前的内容、片段引用起始标记、片段名称、片段引用结束标记以及片段引用之后的内容，在记号列表中取代 \type{t}。该过程由以下辅助函数实现：

@ 词法分析阶段的辅助函数 # +
<split_snippet>
static void split_snippet(WKList *tokens, WKLink *x, WKPair *snippet_reference) {
        WKStr *ref_start_mark = xi_symbols->snippet_reference_start_mark;
        WKStr *ref_stop_mark = xi_symbols->snippet_reference_stop_mark;
        XIToken *t = wk_link_get(x, XIToken *);
        size_t line_number = t->line_number;
        const char *ref_start = wk_box_get(snippet_reference->x, const char *);
        const char *ref_stop = wk_box_get(snippet_reference->y, const char *);
        /* 片段引用之前的内容 */
        WKStr *a = wk_str(NULL);
        for (const char *p = t->content->body; p != ref_start; p++) {
                wk_str_suffix_char(a, *p);
                if (*p == '\n') line_number++;
        }
        XIToken *t_a = malloc(sizeof(XIToken));
        t_a->type = XI_TEXT;
        t_a->line_number = t->line_number;
        t_a->content = a;
        /* 片段引用起始标记 */
        WKStr *b = wk_str(ref_start_mark->body);
        XIToken *t_b = malloc(sizeof(XIToken));
        t_b->type = XI_SNIPPET_REFERENCE_START_MARK;
        t_b->line_number = line_number;
        t_b->content = b;
        /* 片段名称 */
        WKStr *c = wk_str(NULL);
        const char *left = ref_start + ref_start_mark->n;
        const char *right = ref_stop - ref_stop_mark->n;
        for (const char *p = left; p != right; p++) {
                wk_str_suffix_char(c, *p);
                if (*p == '\n') line_number++;
        }
        XIToken *t_c = malloc(sizeof(XIToken));
        t_c->type = XI_TEXT;
        t_c->line_number = t_b->line_number;
        t_c->content = c;
        /* 片段引用结束标记 */
        WKStr *d = wk_str(ref_stop_mark->body);
        XIToken *t_d = malloc(sizeof(XIToken));
        t_d->type = XI_SNIPPET_REFERENCE_STOP_MARK;
        t_d->line_number = line_number;
        t_d->content = d;
        /* 更新记号列表 */
        WK_LIST_INSF(tokens, x, &t_a);
        WK_LIST_INSF(tokens, x, &t_b);
        WK_LIST_INSF(tokens, x, &t_c);
        WK_LIST_INSF(tokens, x, &t_d);
        /* 更新行号 */
        t->line_number = line_number;
        /* 片段引用之后的内容 */
        WKStr *f = wk_str(NULL);
        for (const char *p = ref_stop; *p != '\0'; p++) wk_str_suffix_char(f, *p);
        wk_str_free(t->content);
        t->content = f;
}
@

假设 \type{xi_tokens} 的遍历指针为 \type{it}，则有名片段中的所有片段引用的解析过程如下：

@ 析取片段引用 #
it = xi_tokens->head;
while (1) {
        if (!it) break;
        XIToken *t = wk_link_get(it, XIToken *);
        if (t->type == XI_TEXT) {
                WKLink *prev = it->prev;
                if (prev) {
                        XIToken *t_prev = wk_link_get(prev, XIToken *);
                        if (t_prev->type != XI_SNIPPET_DELIMITER
                            && t_prev->type != XI_LANGUAGE_START_MARK
                            && t_prev->type != XI_TAG_START_MARK) {
                                while (1) {
                                        # 构造片段引用 @
                                }
                        }
                }
        }
        it = it->next;
}
@

上述代码中，对 \type{t} 中可能存在的片段引用析取过程如下：

@ 构造片段引用 #
WKPair *snippet_reference = find_snippet_reference(t->content);
if (!snippet_reference) break;
else {
        split_snippet(xi_tokens, it, snippet_reference);
        wk_box_free(snippet_reference->x);
        wk_box_free(snippet_reference->y);
        free(snippet_reference);
}
@

\subsection{小节}

汇集词法分析的各阶段代码：

@ xi 的数据结构与辅助函数 # [C]
# 记号 @
# 词法分析阶段的辅助函数 @
# 辅助函数 delete_tokens，用于释放记号列表 @
static WKList *xi_lexer(FILE *xi_file) {
        # 准备可忽略的空白字符集 @
        # 分段 @
        # 析取片段名界限符 @
        # 析取语言标记 @
        # 析取标签引用和运算符 @
        # 析取标签 @
        # 析取片段引用 @
        return xi_tokens;
}
@

上述代码片段所用的辅助函数 \type{delete_tokens}，其定义为

@ 辅助函数 delete_tokens，用于释放记号列表 #
static void delete_tokens(WKList *tokens) {
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                wk_str_free(t->content);
                free(t);
        }
        wk_list_free(tokens);
}
@


函数 \type{xi_print_tokens}，可用于查看词法分析结果，其定义如下：

@ xi_print_tokens # [C]
static void xi_print_tokens(WKList *tokens) {
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                switch (t->type) {
                case XI_SNIPPET_DELIMITER:
                        printf("snippet delimiter");
                        break;
                case XI_SNIPPET_NAME_DELIMITER:
                        printf("snippet name delimiter");
                        break;
                case XI_LANGUAGE_START_MARK:
                        printf("language start mark");
                        break;
                case XI_LANGUAGE_STOP_MARK:
                        printf("language end mark");
                        break;
                case XI_SNIPPET_APPENDING_MARK:
                        printf("snippet appending mark");
                        break;
                case XI_SNIPPET_PREPENDING_MARK:
                        printf("snippet prepending mark");
                        break;
                case XI_TAG_START_MARK:
                        printf("tag start mark");
                        break;
                case XI_TAG_STOP_MARK:
                        printf("tag end mark");
                        break;
                case XI_SNIPPET_REFERENCE_START_MARK:
                        printf("snippet reference start mark");
                        break;
                case XI_SNIPPET_REFERENCE_STOP_MARK:
                        printf("snippet reference stop mark");
                        break;
                case XI_TEXT:
                        printf("text");
                        break;
                default:
                        printf("Illegal token!\n");
                }
                printf(" | line %lu: %s\n", t->line_number, t->content->body);
        }
}
@

\section{语法}

基于词法分析所得记号列表可构造语法树。语法树中存储的数据为语法单元，其数据结构如下：

@ 语法单元 # [C]
typedef struct {
        XISyntaxType type;
        WKList *tokens;
        size_t id;
} XISyntax;
@

\noindent \type{type} 字段表示语法节点的类型，具体有以下类型：

@ 语法单元 # ^+
typedef enum {
        XI_SNIPPET,
        XI_SNIPPET_WITH_NAME,
        XI_SNIPPET_NAME,
        XI_SNIPPET_LANGUAGE,
        XI_SNIPPET_APPENDING_OPERATOR,
        XI_SNIPPET_PREPENDING_OPERATOR,
        XI_SNIPPET_TAG_REFERENCE,
        XI_SNIPPET_TAG,
        XI_SNIPPET_CONTENT,
        XI_SNIPPET_REFERENCE,
        XI_SNIPPET_TEXT
} XISyntaxType;
@

\noindent \type{tokens} 为构成语法节点的记号列表，词法分析输出的记号列表会在语法树的构造过程中逐步被分解到语法树的叶层节点里。非叶层节点的 \type{tokens} 字段为 \type{NULL}。\type{id} 字段只对有名片段有意义，且从 1 开始逐一递增，对于无名片段以及其他类型的语法节点，其值为 0。

在语法分析过程中，所构建的语法节点依序存入由 WK 库的 WKTree 模块提供的 N 叉树容器，先引入该模块的头文件：

@ 外境 # +
#include <wk-tree.h>
@

语法树的构造过程分为三个阶段。第一阶段，构造无名片段和有名片段节点。第二阶段，对有名片段进行结构化处理。第三阶段，对有名片段的内容进行结构化处理。注意，语法树的根节点的数据字段为 \type{NULL}。这三个阶段构造了四层的语法树结构。

\subsection{片段}

根据记号列表中的片段界限符以及片段名称界限符便可区分两类片段形式，并在语法树中为其构造节点：

@ 语法分析过程所需的辅助函数 # [C]
static WKTree *syntax_tree_init(WKList *tokens) {
        WKTree *xi_tree = wk_tree(XISyntax *);
        wk_tree_add(xi_tree, NULL, NULL, XISyntax *); /* 根节点为空 */
        size_t id = 1; /* 有名片段的 id */
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_SNIPPET_DELIMITER) {
                        WKLink *prev = it->prev;
                        if (prev) {
                                XISyntax *x = malloc(sizeof(XISyntax));
                                x->type = XI_SNIPPET; /* 默认类型，后续会确定它是否为有名片段 */
                                x->tokens = wk_list(XIToken *);
                                x->id = 0;
                                # 为片段收集记号并确定片段类型 @
                                /* 注意，词法分析阶段，在记号列表尾部强行插入了一个片段界限记号，
                                   若该记号之前也是片段界限记号，会导致 x 的内容为空，需要予以忽略 */
                                if (x->tokens) {
                                        wk_tree_add(xi_tree, xi_tree->root, x, XISyntax *);
                                } else free(x);
                        }
                }
        }
        /* 释放片段界限记号 */
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_SNIPPET_DELIMITER) {
                        wk_str_free(t->content);
                        free(t);
                }
        }
        /* 有用的记号皆已转移到语法树，销毁记号列表容器已完成使命 */
        wk_list_free(tokens);
        return xi_tree;
}
@

从 \type{prev} 所指记号开始，逆序遍历记号列表，为新建片段 \type{x} 收集记号并确定片段类型，该过程为

@ 为片段收集记号并确定片段类型 #
WKLink *a = prev;
while (1) {
        if (a) {
                XIToken *x_tok = wk_link_get(a, XIToken *);
                if (x_tok->type == XI_SNIPPET_DELIMITER) break;
                else {
                        if (x_tok->type == XI_SNIPPET_NAME_DELIMITER) {
                                x->type = XI_SNIPPET_WITH_NAME;
                                x->id = id++;
                        }
                        wk_list_prefix(x->tokens, x_tok, XIToken *);
                }
        } else break;
        a = a->prev;
}
@

\subsection{有名片段的子节点集}

假设语法树的第二层节点中，节点 \type{x} 为有名片段节点，则需基于其记号序列构造语法节点，且所有记号会被转移到后者之中。该过程结束后，语法树第 2 层的有名片段节点中的语法单元便不再持有记号序列，亦即记号序列字段的值为空指针。

首先，对于语法树 \type{syntax_tree} 中任一有名片段的语法节点 \type{x_bra}，初始化迭代指针 \type{it}，使之指向 \type{x_bra} 中语法单元 \type{x} 的记号序列首节点，并获得当前记号 \type{x_tok}：

@ 有名片段的子节点集构造过程 #
XISyntax *x = wk_branch_get(x_bra, XISyntax *);
WKLink *it = x->tokens->head;
XIToken *x_tok = wk_link_get(it, XIToken *);
@

有名片段的第一个记号应当是片段名，其语法节点构造过程如下：

@ 有名片段的子节点集构造过程 # +
XISyntax *x_name = malloc(sizeof(XISyntax));
x_name->type = XI_SNIPPET_NAME;
x_name->tokens = wk_list(XIToken *);
x_name->id = 0;
wk_list_suffix(x_name->tokens, x_tok, XIToken *);
wk_tree_add(syntax_tree, x_bra, x_name, XISyntax *);
@

移动 \type{it} 使之跳过片段名界限符，然后构造可能存在的语言标记节点：

@ 有名片段的子节点集构造过程 # +
it = it->next->next; /* 跳过片段名字界限符 */
x_tok = wk_link_get(it, XIToken *);
if (x_tok->type == XI_LANGUAGE_START_MARK) {
        XISyntax *x_lang = malloc(sizeof(XISyntax));
        it = it->next;
        x_tok = wk_link_get(it, XIToken *);
        x_lang->type = XI_SNIPPET_LANGUAGE;
        x_lang->tokens = wk_list(XIToken *);
        x_lang->id = 0;
        wk_list_suffix(x_lang->tokens, x_tok, XIToken *);
        wk_tree_add(syntax_tree, x_bra, x_lang, XISyntax *);
        it = it->next;
        x_tok = wk_link_get(it, XIToken *);
        it = it->next; /* 跳过语言标记终结符 */
}
@

然后构造可能存在的标签引用节点：

@ 有名片段的子节点集构造过程 # +
x_tok = wk_link_get(it, XIToken *);
if (x_tok->type == XI_TAG_START_MARK) {
        bool is_tag_ref = false;
        for (WKLink *it_a = it; it_a; it_a = it_a->next) {
               XIToken *t = wk_link_get(it_a, XIToken *);
                if (t->type == XI_SNIPPET_APPENDING_MARK
                    || t->type == XI_SNIPPET_PREPENDING_MARK) {
                        is_tag_ref = true;
                        break;
                }
        }
        if (is_tag_ref) {
                XISyntax *x_tag_ref = malloc(sizeof(XISyntax));
                it = it->next;
                x_tok = wk_link_get(it, XIToken *);
                x_tag_ref->type = XI_SNIPPET_TAG_REFERENCE;
                x_tag_ref->tokens = wk_list(XIToken *);
                x_tag_ref->id = 0;
                wk_list_suffix(x_tag_ref->tokens, x_tok, XIToken *);
                wk_tree_add(syntax_tree, x_bra, x_tag_ref, XISyntax *);
                /* 跳过标签引用结束符 */
                it = it->next->next;
        }
}
@

继而构造可能存在的运算符节点：

@ 有名片段的子节点集构造过程 # +
x_tok = wk_link_get(it, XIToken *);
XISyntax *x_op = NULL;
if (x_tok->type == XI_SNIPPET_APPENDING_MARK) {
        x_op = malloc(sizeof(XISyntax));
        x_op->type = XI_SNIPPET_APPENDING_OPERATOR;
        x_op->id = 0;
}
if (x_tok->type == XI_SNIPPET_PREPENDING_MARK) {
        x_op = malloc(sizeof(XISyntax));
        x_op->type = XI_SNIPPET_PREPENDING_OPERATOR;
        x_op->id = 0;
}
if (x_op) {
        x_op->tokens = wk_list(XIToken *);
        wk_list_suffix(x_op->tokens, x_tok, XIToken *);
        wk_tree_add(syntax_tree, x_bra, x_op, XISyntax *);
        it = it->next;
}
@

之后便是构造可能存在的标签节点：

@ 有名片段的子节点集构造过程 # +
x_tok = wk_link_get(it, XIToken *);
if (x_tok->type == XI_TAG_START_MARK) {
        XISyntax *x_tag = malloc(sizeof(XISyntax));
        it = it->next;
        x_tok = wk_link_get(it, XIToken *);
        x_tag->type = XI_SNIPPET_TAG;
        x_tag->tokens = wk_list(XIToken *);
        x_tag->id = 0;
        wk_list_suffix(x_tag->tokens, x_tok, XIToken *);
        wk_tree_add(syntax_tree, x_bra, x_tag, XISyntax *);
        /* 跳过标签结束符 */
        it = it->next->next;
}
@

余下部分，便是有名片段的内容部分，其语法节点构造过程如下：

@ 有名片段的子节点集构造过程 # +
XISyntax *x_content = malloc(sizeof(XISyntax));
x_content->type = XI_SNIPPET_CONTENT;
x_content->tokens = wk_list(XIToken *);
x_content->id = 0;
for (WKLink *it_a = it; it_a; it_a = it_a->next) {
        x_tok = wk_link_get(it_a, XIToken *);
        wk_list_suffix(x_content->tokens, x_tok, XIToken *);
}
wk_tree_add(syntax_tree, x_bra, x_content, XISyntax *);
@

一些不需要进入语法树的记号，需要从记号列表中剔除：

@ 释放不需要进入语法树的记号 #
for (it = x->tokens->head; it; it = it->next) {
        x_tok = wk_link_get(it, XIToken *);
        if (x_tok->type == XI_SNIPPET_NAME_DELIMITER
            || x_tok->type == XI_LANGUAGE_START_MARK
            || x_tok->type == XI_LANGUAGE_STOP_MARK
            || x_tok->type == XI_TAG_START_MARK
            || x_tok->type == XI_TAG_STOP_MARK) {
                wk_str_free(x_tok->content);
                free(x_tok);
        }
}
@

至此，有名片段语法单元 \type{snippet} 里的记号序列，其内容已全部转移至其子节点集，故而需将其置空：

@ 清空有名片段语法单元的记号序列 #
wk_list_free(x->tokens);
x->tokens = NULL;
@

将上述过程汇总为一个专用的辅助函数：

@ 语法分析过程所需的辅助函数 # +
static void add_branches_for_snippet_with_name(WKTree *syntax_tree, WKBranch *x_bra) {
        # 有名片段的子节点集构造过程 @
        # 释放不需要进入语法树的记号 @
        # 清空有名片段语法单元的记号序列 @
}
@

设语法树为 \type{xi_tree}，为语法树中第 2 层\footnote{语法树根节点为第 1 层。}所有类型为 \type{XI_SNIPPET_WITH_NAME} 的片段节点构造子节点集的代码如下：

@ 为有名片段构造子节点集 # [C]
WKArray *branches = xi_tree->root->lower;
for (size_t i = 0; i < branches->n; i++) {
        WKBranch *x_bra = wk_array_get(branches, i, WKBranch *);
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        if (x->type == XI_SNIPPET_WITH_NAME) {
                add_branches_for_snippet_with_name(xi_tree, x_bra);
        }
}
@

\subsection{有名片段的内容}

有名片段的内容节点仅包含两种语法节点类型：

\startitemize
\item \type{XI_SNIPPET_REFERENCE}
\item \type{XI_SNIPPET_TEXT}
\stopitemize

\noindent 假设语法树的第三层节点 \type{x_bra} 中语法单元 \type{x} 的类型为 \type{XI_SNIPPET_CONTENT}，需为其构造子节点集，过程如下：

@ 语法分析过程所需的辅助函数 # +
static void add_branches_for_snippet_content(WKTree *syntax_tree, WKBranch *x_bra) {
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        WKLink *it = x->tokens->head;
        while (it) {
                XIToken *x_tok = wk_link_get(it, XIToken *);
                if (x_tok->type == XI_SNIPPET_REFERENCE_START_MARK) {
                        it = it->next;
                        x_tok = wk_link_get(it, XIToken *);
                        /* 构造片段引用 */
                        XISyntax *ref = malloc(sizeof(XISyntax));
                        ref->type = XI_SNIPPET_REFERENCE;
                        ref->tokens = wk_list(XIToken *);
                        ref->id = 0;
                        wk_list_suffix(ref->tokens, x_tok, XIToken *);
                        wk_tree_add(syntax_tree, x_bra, ref, XISyntax *);
                        /* 让片段引用结束符作为当前节点 */
                        it = it->next;
                } else {
                        XISyntax *text = malloc(sizeof(XISyntax));
                        text->type = XI_SNIPPET_TEXT;
                        text->tokens = wk_list(XIToken *);
                        text->id = 0;
                        wk_list_suffix(text->tokens, x_tok, XIToken *);
                        wk_tree_add(syntax_tree, x_bra, text, XISyntax *);
                }
                it = it->next;
        }
        /* 释放 x->tokens */
        for (it = x->tokens->head; it; it = it->next) {
                XIToken *x_tok = wk_link_get(it, XIToken *);
                if (x_tok->type == XI_SNIPPET_REFERENCE_START_MARK
                    || x_tok->type == XI_SNIPPET_REFERENCE_STOP_MARK) {
                        wk_str_free(x_tok->content);
                        free(x_tok);
                }
        }
        wk_list_free(x->tokens);
        x->tokens = NULL;
}
@

为语法树 \type{xi_tree} 第三层所有类型为 \type{XI_SNIPPET_CONTENT} 的节点构造子节点集，

@ 为有名片段的内容节点构造子节点集 #
WKArray *second_level_branches = xi_tree->root->lower;
for (size_t i = 0; i < second_level_branches->n; i++) {
        WKBranch *x_bra = wk_array_get(second_level_branches, i, WKBranch *);
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        if (x->type == XI_SNIPPET_WITH_NAME) {
                WKArray *third_level_branches = x_bra->lower;
                for (size_t j = 0; j < third_level_branches->n; j++) {
                        WKBranch *y_bra = wk_array_get(third_level_branches, j, WKBranch *);
                        XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                        if (y->type == XI_SNIPPET_CONTENT) {
                                add_branches_for_snippet_content(xi_tree, y_bra);
                        }
                }
        }
}
@

\subsection{语法树访问与释放}

\type{first_token_line_number} 可以获取语法树中任一节点内容的首行行号，其定义为

@ first_token_line_number # [C]
static size_t first_token_line_number(WKBranch *x) {
        size_t n = 0;
        XISyntax *a = wk_branch_get(x, XISyntax *);
        if (a->tokens) {
                XIToken *t = wk_link_get(a->tokens->head, XIToken *);
                n = t->line_number;
        } else {
                if (x->lower) {
                        n = first_token_line_number(wk_array_get(x->lower,
                                                                 0,
                                                                 WKBranch *));
                } else {
                        fprintf(stderr, "Illlegal syntax tree!\n");
                }
        }
        return n;
}
@

\type{xi_print_syntax_tree} 用于查看语法分析结果，其定义为

@ xi_print_syntax_tree # [C]
# xi_print_tokens @
static void xi_print_syntax_tree(WKBranch *root) {
        if (!root) return;
        XISyntax *a = wk_branch_get(root, XISyntax *);
        if (a) {
                switch (a->type) {
                case XI_SNIPPET:
                        printf("<snippet>\n");
                        break;
                case XI_SNIPPET_WITH_NAME:
                        printf("<snippet with name: %lu>\n", a->id);
                        break;
                case XI_SNIPPET_NAME:
                        printf("<name>\n");
                        break;
                case XI_SNIPPET_LANGUAGE:
                        printf("<language>\n");
                        break;
                case XI_SNIPPET_APPENDING_OPERATOR:
                        printf("<snippet appending operator>\n");
                        break;
                case XI_SNIPPET_PREPENDING_OPERATOR:
                        printf("<snippet prepending operator>\n");
                        break;
                case XI_SNIPPET_TAG_REFERENCE:
                        printf("<snippet tag reference>\n");
                        break;
                case XI_SNIPPET_TAG:
                        printf("<snippet tag>\n");
                        break;
                case XI_SNIPPET_CONTENT:
                        printf("<snippet content>\n");
                        break;
                case XI_SNIPPET_REFERENCE:
                        printf("<snippet reference>\n");
                        break;
                case XI_SNIPPET_TEXT:
                        printf("<snippet text>\n");
                        break;
                default:
                        fprintf(stderr, "Illegal syntax in %lu.\n",
                                        first_token_line_number(root));
                        exit(EXIT_FAILURE);
                }
                if (a->tokens) {
                        xi_print_tokens(a->tokens);
                }
        }
        if (root->lower) {
                for (size_t i = 0; i < root->lower->n; i++) {
                        WKBranch *p = wk_array_get(root->lower, i, WKBranch *);
                        xi_print_syntax_tree(p);
                }
        }
}
@

\type{xi_delete_syntax_tree} 函数用于释放语法树内存，其定义为

@ xi_delete_syntax_tree # [C]
static void delete_syntax_body(WKBranch *root) {
        if (!root) return;
        XISyntax *x = wk_branch_get(root, XISyntax *);
        if (x) {
                if (x->tokens) delete_tokens(x->tokens);
                free(x);
        }
        if (root->lower) {
                for (size_t i = 0; i < root->lower->n; i++) {
                        delete_syntax_body(wk_array_get(root->lower, i, WKBranch *));
                }
        }
}
static void xi_delete_syntax_tree(WKTree *syntax_tree) {
        delete_syntax_body(syntax_tree->root);
        wk_tree_free(syntax_tree);
}
@

\subsection{小结}

汇集语法分析相关的代码片段：

@ xi 的数据结构与辅助函数 # +
# 语法单元 @
# 语法分析过程所需的辅助函数 @
# first_token_line_number @
/* # xi_print_syntax_tree @ */
# xi_delete_syntax_tree @ 
static WKTree *xi_parser(WKList *tokens) {
        WKTree *xi_tree = syntax_tree_init(tokens);
        # 为有名片段构造子节点集 @
        # 为有名片段的内容节点构造子节点集 @
        return xi_tree;
}
@

\section{联系}

片段有了名字，就有了最基本的纠缠——一个片段可以引用另一个片段。现在需要为有名片段建立一个关系表，呈现它们之间的引用关系。关系表的数据结构基于 WK 库的 WKTable 模块提供的散列表实现。关系表里存储的数据，其结构为

@ XITie 结构 # [C]
typedef struct {
        WKArray *time_order;
        WKArray *spatial_order;
        WKArray *emissions;
} XITie;
@

\noindent 每一个不同名字的片段都对应一个 \type{XITie} 类型的对象，其自身会被分别存储在 \type{time_order} 和  \type{spatial_order}。同名的片段会被以它们在文学程序中出现次序与逻辑次序（由片段运算符决定）分别存储在 \type{spatial_order} 和 \type{time_order} 中。\type{emissions} 存储片段的引用者。

\subsection{键}

将 \type{XITie} 对象加入有名片段关系表中时，所用的键是片段的名字，但需要取出其中所有空白字符（空格，缩进，换行符以及续行符）和一些特殊符号，辅助函数 \type{compact_text} 可完成此事，

@ 有名片段关系表构建过程所需的辅助函数 # [C]
static WKStr *compact_text(WKStr *a, WKArray *skipped_chars) {
        WKStr *b = wk_str(a->body);
        for (size_t i = 0; i < skipped_chars->n; i++) {
                const char *c = wk_array_get(skipped_chars, i, const char *);
                wk_str_replace(b, c, "");
        }
        return b;
}
@

全局变量 \type{xi_skipped_chars_in_name} 存储了空白字符和续行符，专用作程序片段名中可忽略的字符集，其定义、初始化及释放过程如下：

@ 全局变量 # +
WKArray *xi_skipped_chars_in_name;
@

@ 构造冗白字符集 xi_skipped_chars_in_name #
xi_skipped_chars_in_name = wk_array(const char *);
wk_array_add(xi_skipped_chars_in_name, " ", const char *);
wk_array_add(xi_skipped_chars_in_name, "　", const char *);
wk_array_add(xi_skipped_chars_in_name, "\t", const char *);
wk_array_add(xi_skipped_chars_in_name, "\n", const char *);
wk_array_add(xi_skipped_chars_in_name,
             xi_symbols->snippet_name_continuation->body,
             const char *);
wk_array_add(xi_skipped_chars_in_name, "\'", const char *);
wk_array_add(xi_skipped_chars_in_name, "\"", const char *);
/* 如有需要，可继续添加 */
/* ... ... ... */
@

@ xi 进程资源回收 # +
wk_array_free(xi_skipped_chars_in_name);
@

\subsection[relations]{关系表初始化}

对于语法树中任一有名片段节点 \type{x}，将其存入表 \type{relations} 的过程如下

@ 有名片段关系表构建过程所需的辅助函数 # +
static void init_tie(WKTable *relations, WKBranch *x_bra) {
        WKStr *x_name = NULL;
        # 从 x_bra 的子节点获取片段名并将其存于 x_name @
        if (!x_name) {
                fprintf(stderr, "Line %lu: Illegal snipet.\n", first_token_line_number(x_bra));
                exit(EXIT_FAILURE);
        }
        WKStr *key = compact_text(x_name, xi_skipped_chars_in_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);        
                bool has_appending_operator = false;
                bool has_prepending_operator = false;
                WKStr *x_tag_ref = NULL;
                # 检测 x_bra 是否含有标签引用和运算符 @
                if (x_tag_ref) {
                        WKStr *t = compact_text(x_tag_ref, xi_skipped_chars_in_name);
                        int id = -1;
                        # 从时间（程序逻辑）序列中寻找与 t 相同的标签 -> id @
                        # 根据运算符，在时间序列中，\
                          将 x_bra 插入到 t 对应的同名节点之前或之后 @
                        wk_str_free(t);
                } else {
                        # 根据运算符，在时间序列中，\
                          将 x_bra 插入到同名节点之前或之后 @
                }
                wk_array_add(tie->spatial_order, x_bra, WKBranch *);
                wk_str_free(key);
        } else { /* 将 x_bra 添加至 relations */
                XITie *tie = malloc(sizeof(XITie));
                tie->time_order = wk_array(WKBranch *);
                tie->spatial_order = wk_array(WKBranch *);
                tie->emissions = wk_array(WKBranch *);
                wk_array_add(tie->time_order, x_bra, WKBranch *);
                wk_array_add(tie->spatial_order, x_bra, WKBranch *);
                wk_table_add(relations, wk_pair(wk_box(key, WKStr *), wk_box(tie, XITie *)));
        }
}
@

\noindent 单从语法而言，\type{x} 的第 1 个子节点便应是片段名节点，但稳妥起见，获取片段名的过程依然是用了穷举的方式，且后续检测 \type{x} 是否含有标签引用和运算符的过程也沿袭此法。

@ 从 x_bra 的子节点获取片段名并将其存于 x_name #
for (size_t i = 0; i < x_bra->lower->n; i++) {
        WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
        XISyntax *y = wk_branch_get(y_bra, XISyntax *);
        if (y->type == XI_SNIPPET_NAME) {
                XIToken *x_name_tok = wk_link_get(y->tokens->head, XIToken *);
                x_name = x_name_tok->content;
                break;
        }
}
@

@ 检测 x_bra 是否含有标签引用和运算符 #
for (size_t i = 0; i < x_bra->lower->n; i++) {
        WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
        XISyntax *y = wk_branch_get(y_bra, XISyntax *);
        if (y->type == XI_SNIPPET_TAG_REFERENCE) {
                XIToken *x_tag_ref_tok = wk_link_get(y->tokens->head, XIToken *);
                x_tag_ref = x_tag_ref_tok->content;
        }
        if (y->type == XI_SNIPPET_APPENDING_OPERATOR) {
                has_appending_operator = true;
                break;
        }
        if (y->type == XI_SNIPPET_PREPENDING_OPERATOR) {
                has_prepending_operator = true;
                break;
        }
}
@

标签引用连同运算符 \type{+} 和 \type{^+} 所起的作用是确定同名片段在时间序列里的顺序。以下过程决定了，若一个含有标签的片段，若它在空间序列中位于 \type{x} 之后，则 \type{x} 无法通过标签和运算符确定 \type{x} 在时间序列中的位置，原因是前者尚未出现在 \type{x} 同名片段的时间序列里。

@ 从时间（程序逻辑）序列中寻找与 t 相同的标签 -> id #
size_t hits = 0;
for (int i = 0; i < tie->time_order->n; i++) {
        WKBranch *y_bra = wk_array_get(tie->time_order, i, WKBranch *);
        for (size_t j = 0; j < y_bra->lower->n; j++) {
                WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
                XISyntax *z = wk_branch_get(z_bra, XISyntax *);
                if (z->type == XI_SNIPPET_TAG) {
                        XIToken *x_tag_tok = wk_link_get(z->tokens->head, XIToken *);
                        WKStr *s = compact_text(x_tag_tok->content, xi_skipped_chars_in_name);
                        if (strcmp(s->body, t->body) == 0) {
                                id = i;
                                hits++;
                        }
                        wk_str_free(s);
                }
        }
}
if (hits > 1) {
        fprintf(stderr, "Line %lu: repeated tag <%s>!\n", first_token_line_number(x_bra), t->body);
        exit(EXIT_FAILURE);
}
if (id < 0) {
        fprintf(stderr, "Line %lu: The referenced tag does not exist.\n", first_token_line_number(x_bra));
        exit(EXIT_FAILURE);
}
@

@ 根据运算符，在时间序列中，\
  将 x_bra 插入到 t 对应的同名节点之前或之后 #
if (has_appending_operator) wk_array_ins(tie->time_order, id + 1, x_bra, WKBranch *);
else if (has_prepending_operator) wk_array_ins(tie->time_order, id, x_bra, WKBranch *);
else {
        fprintf(stderr, "Line %lu: The snippet needs an operator.\n",
                        first_token_line_number(x_bra));
        exit(EXIT_FAILURE);
}
@

@ 根据运算符，在时间序列中，将 x_bra 插入到同名节点之前或之后 #
if (has_appending_operator) wk_array_add(tie->time_order, x_bra, WKBranch *);
else if (has_prepending_operator) wk_array_ins(tie->time_order, 0, x_bra, WKBranch *);
else {
        fprintf(stderr, "Line %lu: The snippet needs an operator.\n",
                        first_token_line_number(x_bra));
        exit(EXIT_FAILURE);
}
@

\subsection{建立有名片段的引用关系}

假设任一有名片段节点 \type{x_bra}，其中含有对其他片段的引用 \type{a}，则以下过程可在有名片段关系表中为二者建立联系：

@ 有名片段关系表构建过程所需的辅助函数 # +
static void create_relation(WKTable *relations, WKBranch *x_bra, XISyntax *a)
{
        XIToken *a_tok = wk_link_get(a->tokens->head, XIToken *);
        WKStr *s = compact_text(a_tok->content, xi_skipped_chars_in_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(s, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);
                wk_array_add(tie->emissions, x_bra, WKBranch *);
        } else {
                fprintf(stderr, "Line %lu: The snippet named <%s> not existed.\n",
                                a_tok->line_number, a_tok->content->body);
                exit(EXIT_FAILURE);
        }
        wk_str_free(s);
}
@

基于 \type{init_tie} 和 \type{create_relation}，构造有名片段关系的完整过程如下：

@ 有名片段关系表构建过程所需的辅助函数 # +
/* root 为语法树根节点 */
static WKTable *xi_create_relations(WKBranch *root) {
        WKTable *relations = wk_table(WKStr *, XITie *);
        for (size_t i = 0; i < root->lower->n; i++) {
                WKBranch *x_bra = wk_array_get(root->lower, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type != XI_SNIPPET_WITH_NAME) continue;
                init_tie(relations, x_bra);
        }
        for (size_t i = 0; i < root->lower->n; i++) {
                WKBranch *x_bra = wk_array_get(root->lower, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type != XI_SNIPPET_WITH_NAME) continue;
                WKBranch *y_bra = wk_array_get(x_bra->lower, x_bra->lower->n - 1, WKBranch *);
                for (size_t j = 0; j < y_bra->lower->n; j++) {
                        WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
                        XISyntax *z = wk_branch_get(z_bra, XISyntax *);
                        if (z->type != XI_SNIPPET_REFERENCE) continue;
                        create_relation(relations, x_bra, z);
                }
        }
        return relations;
}
@

\subsection{访问关系表}

函数 \type{xi_print_relations} 用于输出有名片段关系表的构建结果，其定义为

@ xi_print_relations # [C]
static void xi_print_relations(WKTable *relations) {
        for (size_t i = 0; i < relations->body->n; i++) {
                WKList *bucket = wk_array_get(relations->body, i, WKList *);
                for (WKLink *it = bucket->head; it; it = it->next) {
                        WKPair *entry = wk_link_get(it, WKPair *);
                        WKStr *key = wk_box_get(entry->x, WKStr *);
                        XITie *tie = wk_box_get(entry->y, XITie *);
                        printf("<key>%s</key>\n", key->body);
                        printf("<value>\n"
                               "<spatial_order>\n");
                        for (size_t i = 0; i < tie->spatial_order->n; i++) {
                                print_snippet_name(wk_array_get(tie->spatial_order,
                                                                i,
                                                                WKBranch *));
                        }
                        printf("</spatial_order>\n");
                        printf("<time_order>\n");
                        for (size_t i = 0; i < tie->time_order->n; i++) {
                                print_snippet_name(wk_array_get(tie->time_order,
                                                                i,
                                                                WKBranch *));
                        }
                        printf("</time_order>\n");
                        printf("<emissions>");
                        for (size_t i = 0; i < tie->emissions->n; i++) {
                                print_snippet_name(wk_array_get(tie->emissions,
                                                                i,
                                                                WKBranch *));
                        }
                        printf("</emmisions>\n</value>\n");        
                }
        }
}
@

\subsection{销毁关系表}

关系表不再使用时，可使用 \type{wk_table_free} 予以销毁，但需要事先为表中键值对的值部分注册释放函数：

@ 为关系表注册 XITie 对象释放函数 #
wk_free_bus_connect("XITie *", xi_tie_box_free);
@

\noindent 其中 \type{xi_tie_box_free} 的定义如下：

@ XITie 对象的释放函数：xi_tie_free  #
static void xi_tie_box_free(WKBox *tie_box) {
        XITie *tie = wk_box_get(tie_box, XITie *);
        wk_array_free(tie->spatial_order);
        wk_array_free(tie->time_order);
        wk_array_free(tie->emissions);
        free(tie);
        wk_box_free(tie_box);
}
@

上述代码所用的辅助函数 \type{print_snippet_name}，其定义为

@ xi_print_relations # ^+
static void print_snippet_name(WKBranch *x) {
        for (size_t i = 0; i < x->lower->n; i++) {
                WKBranch *it = wk_array_get(x->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(it, XISyntax *);
                if (a->type == XI_SNIPPET_NAME) {
                        XIToken *t = wk_link_get(a->tokens->head, XIToken *);
                        printf("<snippet-name>%s %zu</snippet-name>\n",
                               t->content->body,
                               t->line_number);
                        break;
                }
        }
}
@

\subsection{小结}

汇集有名片段关系表构造过程的相关代码：

@ xi 的数据结构与辅助函数 # +
# XITie 结构 @
# 有名片段关系表构建过程所需的辅助函数 @
# XITie 对象的释放函数：xi_tie_free @
@

\section{抽取}

对于文学程序文件 \type{xi_file_path}，根据有名片段关系表 \type{relations}，以给定的入口 \type{entrance}——某个有名片段的名字，抽取片段所包含的所有内容——片段及其所引用的其他片段的内容。由于被某个片段引用的片段又可能引用其他片段，因此片段内容抽取过程是递归的，\type{xi_tangle} 实现了该过程。

@ xi_tangle #
static void xi_tangle(const char *xi_file_path,
                      WKTree *syntax_tree,
                      WKTable *relations,
                      WKStr *entrance,
                      size_t here,  /* 递归路径上节点 id 值的最大值，初值为 0 */
                      WKList *indents, /* 用于记录所引用的片段的缩进层次 */
                      bool show_line_number, /* 用于控制输出的内容是否包含行号 */
                      WKStr *cache) {
        WKStr *key = compact_text(entrance, xi_skipped_chars_in_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (!tie_box) {
                fprintf(stderr, "Snippet <%s> never existed!\n", entrance->body);
		exit(EXIT_FAILURE);
        }
	XITie *tie = wk_box_get(tie_box, XITie *);
        /* 获得入口片段的 id */
        if (here == 0) {
                WKBranch *last_bra = wk_array_get(tie->spatial_order,
                                                  tie->spatial_order->n - 1,
                                                  WKBranch *);
                XISyntax *last = wk_branch_get(last_bra, XISyntax *);
                here = last->id;
        }
        for (size_t i = 0; i < tie->time_order->n; i++) {
                WKBranch *x_bra = wk_array_get(tie->time_order, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                WKBranch *y_bra = wk_array_get(x_bra->lower, x_bra->lower->n - 1, WKBranch *);
                WKStr *current_indent = cascade_indents(indents); /* 将各层次的缩进串联起来 */
                if (i > 0) {
                        if (x->id > here) {
                                wk_str_free(current_indent);
                                continue; /* 同名片段，会被 here 截断 */
                        }
                        /* 同名片段，从第 2 个开始，头部增加换行和缩进 */
                        wk_str_suffix(cache, "\n");
                        wk_str_suffix(cache, current_indent->body);
                }
                size_t last_pos = y_bra->lower->n - 1;
		for (size_t j = 0; j < y_bra->lower->n; j++) {
			WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
                        XISyntax *z = wk_branch_get(z_bra, XISyntax *);
                        XIToken *z_tok = wk_link_get(z->tokens->head, XIToken *);
                        if (z->type == XI_SNIPPET_REFERENCE) {
                                # 输出引用的片段内容 @
                        } else {
                                # 输出片段内普通文本 @
			}
                }
                wk_str_free(current_indent);
        }
        wk_str_free(key);
}
@

\type{cascade_indents} 用于串联各层次缩进，

@ 有名片段内容抽取过程所需的辅助函数 # ^+
static WKStr *cascade_indents(WKList *indents) {
        WKStr *current_indent = wk_str(NULL);
        for (WKLink *it = indents->head; it; it = it->next) {
                WKStr *a = wk_link_get(it, WKStr *);
                wk_str_suffix(current_indent, a->body);
        }
        return current_indent;
}
@

向文件写入片段内容时，构造一个缓冲区，收集待写入的内容，然后将其一次性写入文件。需要注意的是，有名片段的首尾通常各存在一个空行，它们的作用是在视觉上让有名片段的内容更加明显突出，在抽取片段内容时，需要将其忽略。另外，在收集内容之前，由于片段可能是被其他片段引用，在被引用处可能存在缩进，故而需要构造缩进文本，将其添加到片段内容的每一行的行首，从而形成整个片段的缩进，该过程对于含有诸如 Python、YAML 之类代码的片段是必须的。向文件写入片段内容的过程具体如下：

@ 输出片段内普通文本 #
WKStr *text = z_tok->content;
const char *head = text->body;
const char *tail = text->body + text->n;
if (show_line_number) {
        # 输出行号 @
}
/* 首节点掐头 */
if (j == 0) {
        head = xi_skip(head, tail, head, 1, xi_skipped_chars);
        if (*head == '\n') head++;
}
/* 尾节点去尾 */
if (j == last_pos) {
        tail = xi_skip(head, tail, tail, -1, xi_skipped_chars);
        if (*(tail - 1) == '\n') tail--;
}
/* 有时会出现片段内容为空行的情况，此时 head > tail，需要排除这种情况 */
if (head < tail) {
        /* 为其余各行添加缩进 */
        for (const char *p = head; p != tail; p++) {
                wk_str_suffix_char(cache, *p);
                if (*p == '\n') {
                        wk_str_suffix(cache, current_indent->body);
                }
        }
}
@

在每处普通文本首行之前，以 \type{#line 行号 "文件名"} 格式显示行号，且行号需要插入到 \type{cache} 最后一行的行首，

@ 输出行号 #
WKStr *cache_tail = wk_str(NULL);
const char *p = cache->body + cache->n;
p = xi_skip(cache->body, cache->body + cache->n, p, -1, xi_skipped_chars);
if (p > cache->body) {
        if (*(p - 1) == '\n') {
                for (const char *q = p; *q != '\0'; q++) wk_str_suffix_char(cache_tail, *q);
                wk_str_del(cache, cache->n - cache_tail->n, cache_tail->n);
        }
}
WKStr *line_number_text = wk_str(NULL);
size_t line_number = first_token_line_number(z_bra);
p = text->body;
p = xi_skip(text->body, text->body + text->n, p, 1, xi_skipped_chars);
if (*p == '\n') { /* 片段去尾后，会导致行号文本不正常，需作特殊处理 ! */
        line_number++;
}
/* 保证行号文本独处一行 */
if (j == 0) wk_str_printf(line_number_text, "#line %lu \"%s\"\n", line_number, xi_file_path);
else wk_str_printf(line_number_text, "\n#line %lu \"%s\"", line_number, xi_file_path);
wk_str_suffix(cache, line_number_text->body);
if (cache_tail->n > 0) wk_str_suffix(cache, cache_tail->body);
wk_str_free(line_number_text);
wk_str_free(cache_tail);
@

输出引用的片段内容，本质上是递归调用上述的片段内容的输出过程，但是需要为该过程构造正确的缩进层次，否则抽取的内容会出现缩进混乱的情况。引用的片段，其缩进即其前一个 \type{XI_SNIPPET_TEXT} 类型的节点最后一行空白文本，需提取该段文本，以添加到引用的片段内容除首行之外的每行行首，提取过程如下：

@ xi_tangle # ^+
static WKStr *last_blank_line(XISyntax *a) {
        if (a->type != XI_SNIPPET_TEXT) return NULL;
        WKStr *blank_line = wk_str(NULL);
        XIToken *a_tok = wk_link_get(a->tokens->head, XIToken *);
        const char *head = a_tok->content->body;
        const char *tail = head + a_tok->content->n;
        const char *p = tail;
        p = xi_skip(head, tail, p, -1, xi_skipped_chars);
        if (p != head && *(p - 1) == '\n') {
                for (const char *q = p; *q != '\0'; q++) {
                        wk_str_suffix_char(blank_line, *q);
                }
        }
        return blank_line;
}
@

假设引用的片段为 \type{a}，其对应的语法节点为 \type{it}，则为其准备当前层次的缩进文本并递归调用 \type{xi_tangle} 实现引用的片段内容的提取过程为：

@ 输出引用的片段内容 #
if (j > 0) {
        /* 获得缩进 */
	WKBranch *z_prev_bra = wk_array_get(y_bra->lower, j - 1, WKBranch *);
        XISyntax *z_prev = wk_branch_get(z_prev_bra, XISyntax *);
        WKStr *blank_line = last_blank_line(z_prev);
        if (blank_line) {
                wk_list_prefix(indents, blank_line, WKStr *);
        }
        /* 抽取代码 */
        xi_tangle(xi_file_path, syntax_tree, relations,
                  z_tok->content, here, indents, show_line_number, cache);
        /* 删除当前层次的缩进 */
        if (blank_line) {
                wk_str_free(blank_line);
                wk_list_del(indents, indents->head);
        }
}
@

汇集片段内容抽取过程的代码片段：

@ xi 的数据结构与辅助函数 # +
# 有名片段内容抽取过程所需的辅助函数 @
# xi_tangle @
@

\section{编织}

在文学程序里，程序的设计意图在文档片段能得以充分体现，而程序源码片段之间的联系也需要明确呈现出来，这有赖于具体所用的排版语言的交叉引用功能，几乎所有能活到现在的排版语言，例如 HTML、Markdown 以及 \TEX\ 及其衍生格式皆具备该功能，但是需要以配置文件的形式告诉 xi 如何使用。除此之外，若用户希望 xi 能妥善处理一些文学编程符号，也需要以配置文件的形式告诉 xi 如何去做。不过，xi 也可以完全不理睬任何排版语言的需求，只是简单地将语法树和片段关系表中存储的信息转化为 YAML 格式文档，由用户编写相应的后端程序，完成他们所需的排版处理。

\subsection{排版配置}

xi 不局限于排版语言，无名片段可直接输出，但是有名片段的各种标记需要转换为特定排版语言所支持的形式。需要通过 xi 的配置文件将这些形式告知 xi，方能完成文档格式的输出。xi 的配置文件支持以下配置选项：

\startitemize
\item \type{snippet_start}：片段开始标记，可用占位符 \type{${language}}；
\item \type{snippet_stop}：片段结束标记，可用占位符 \type{${language}}；
\item \type{snippet_name_start}：片段名开始标记；
\item \type{snippet_name}：片段名标记，可用占位符 \type{${name}}；
\item \type{snippet_id}：片段 ID，可用占位符 \type{${id}}；
\item \type{snippet_name_stop}：片段名终止标记；
\item \type{snippet_name_continuation}：片段名内续行符标记；
\item \type{snippet_tag}：片段标签，可用占位符 \type{${id}} 和 \type{${name}}，注意此处 \type{id} 是片段 \type{id}，但 \type{name} 是标签名字；
\item \type{snippet_tag_reference}：片段标签引用，可用占位符 \type{${id}} 和 \type{${name}}，注意此处 \type{id} 是片段 \type{id}，但 \type{name} 是标签名字；
\item \type{snippet_reference_start}：片段引用开始标记；
\item \type{snippet_reference}：片段引用标记，可用占位符 \type{${name}}；
\item \type{snippet_reference_id}：片段引用 id 标记，可用占位符 \type{${id}}；
\item \type{snippet_reference_stop}：片段引用终止标记；
\item \type{snippet_emission}：片段引用者标记，可用占位符 \type{${id}} 和 \type{${name}}；
\item \type{snippet_appending_operator}：片段合并运算符标记；
\item \type{snippet_prepending_operator}：片段前向合并运算符标记。
\stopitemize

\noindent 上述配置选项里的占位符，xi 在处理输出时，会用实际内容予以替换。这些选项构成的集合表示为以下结构体类型：

@ 记号 # +
typedef struct {
        WKStr *snippet_start;
        WKStr *snippet_stop;
        WKStr *snippet_name_start;
        WKStr *snippet_name;
        WKStr *snippet_id;
        WKStr *snippet_name_continuation;
        WKStr *snippet_name_stop;
        WKStr *snippet_tag;
        WKStr *snippet_tag_reference;
        WKStr *snippet_reference_start;
        WKStr *snippet_reference;
        WKStr *snippet_reference_id;
        WKStr *snippet_reference_stop;
        WKStr *snippet_emission;
        WKStr *snippet_appending_operator;
        WKStr *snippet_prepending_operator;
} XIFmt;
XIFmt *xi_fmt;
@

xi 配置文件中格式化输出配置选项的解析过程如下：

@ 配置文件解析 # +
xi_fmt = malloc(sizeof(XIFmt));
*xi_fmt = (XIFmt){ wk_str(NULL), wk_str(NULL), wk_str(NULL), wk_str(NULL),
                   wk_str(NULL), wk_str(NULL), wk_str(NULL), wk_str(NULL),
                   wk_str(NULL), wk_str(NULL), wk_str(NULL), wk_str(NULL),
                   wk_str(NULL), wk_str(NULL), wk_str(NULL), wk_str(NULL) };
if (user_config) {
        XI_USER_CONFIG(user_config, xi_fmt, snippet_start);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_stop);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_name_start);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_name);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_id);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_name_continuation);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_name_stop);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_tag);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_tag_reference);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_reference_start);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_reference);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_reference_id);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_reference_stop);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_emission);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_appending_operator);
        XI_USER_CONFIG(user_config, xi_fmt, snippet_prepending_operator);
        /* 将各配置项中可能存在的字面换行符替换为真正的换行符 */
        wk_str_replace(xi_fmt->snippet_start, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_stop, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_name_start, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_name, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_id, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_name_continuation, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_name_stop, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_tag, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_tag_reference, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_reference_start, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_reference, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_reference_id, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_reference_stop, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_emission, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_appending_operator, "\\n", "\n");
        wk_str_replace(xi_fmt->snippet_prepending_operator, "\\n", "\n");
}
@

在 xi 进程退出时，需要回收 \type{xi_fmt} 各字段占用的内存：

@ xi 进程资源回收 # +
wk_str_free(xi_fmt->snippet_start);
wk_str_free(xi_fmt->snippet_stop);
wk_str_free(xi_fmt->snippet_name_start);
wk_str_free(xi_fmt->snippet_name);
wk_str_free(xi_fmt->snippet_id);
wk_str_free(xi_fmt->snippet_name_continuation);
wk_str_free(xi_fmt->snippet_name_stop);
wk_str_free(xi_fmt->snippet_tag);
wk_str_free(xi_fmt->snippet_tag_reference);
wk_str_free(xi_fmt->snippet_reference_start);
wk_str_free(xi_fmt->snippet_reference);
wk_str_free(xi_fmt->snippet_reference_id);
wk_str_free(xi_fmt->snippet_reference_stop);
wk_str_free(xi_fmt->snippet_emission);
wk_str_free(xi_fmt->snippet_appending_operator);
wk_str_free(xi_fmt->snippet_prepending_operator);
free(xi_fmt);
@

\subsection{语言标记传播}

有名片段通常用于存放程序源码。在文学程序的文档排版过程中为了实现程序源码的高亮渲染，需要知道程序原码所属语言，因此每个有名片段皆需要带有语言标记，但是为了方便，xi 只需为属于同一线索的所有的有名片段中的任意一个提供语言标记，然后在织成阶段，将该标记传播至该线索中的其他片段。

辅助函数 \type{get_thread} 可从有名片段关系表中抽取指定线索：

@ 语言标记传播过程所需的辅助函数 # [C]
static void get_thread(WKTable *relations, WKStr *entrance, WKList *thread) {
        WKStr *key = compact_text(entrance, xi_skipped_chars_in_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (!tie_box) {
                fprintf(stderr, "Snippet <%s> never existed!\n", entrance->body);
		exit(EXIT_FAILURE);
        }
        XITie *tie = wk_box_get(tie_box, XITie *);
        for (size_t i = 0; i < tie->spatial_order->n; i++) {
                WKBranch *x_bra = wk_array_get(tie->spatial_order, i, WKBranch *);
                WKBranch *y_bra = wk_array_get(x_bra->lower, x_bra->lower->n - 1, WKBranch *);
                wk_list_prefix(thread, x_bra, WKBranch *);
                for (size_t j = 0; j < y_bra->lower->n; j++) {
                        WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
                        XISyntax *z = wk_branch_get(z_bra, XISyntax *);
                        if (z->type == XI_SNIPPET_REFERENCE) {
                                XIToken *z_tok = wk_link_get(z->tokens->head, XIToken *);
                                get_thread(relations, z_tok->content, thread);
                        }
                }
        }
        wk_str_free(key);
}
@

辅助函数 \type{spread_language_mark_in_thread} 实现任意一条线索中的有名片段的语言标记传播：

@ 语言标记传播过程所需的辅助函数 # +
static void spread_language_mark_in_thread(WKTree *xi_tree, WKList *thread) {
        XIToken *x_lang_tok = NULL;
        for (WKLink *it = thread->head; it; it = it->next) {
                WKBranch *x_bra = wk_link_get(it, WKBranch *);
                x_lang_tok = find_language_token(x_bra);
                if (x_lang_tok) break;
        }
        if (!x_lang_tok) return;
        for (WKLink *it = thread->head; it; it = it->next) {
                WKBranch *x_bra = wk_link_get(it, WKBranch *);
                /* 检测 x_bra 是否含有语言标记 */
                XIToken *lang_tok = find_language_token(x_bra);
                if (lang_tok) {
                        if (strcmp(x_lang_tok->content->body, lang_tok->content->body) != 0) {
                                fprintf(stderr, "Line %lu and %lu: "
                                                "there are two different language marks.\n",
                                                x_lang_tok->line_number, lang_tok->line_number);
                        }
                } else {
                        # 构造语言标记节点 @
                }
        }
}
@

上述代码中使用的 \type{find_language_token} 函数可从语法树中的有名片段节点中搜索语法标记，其定义如下：

@ 语言标记传播过程所需的辅助函数 # ^+
static XIToken *find_language_token(WKBranch *x) {
        XIToken *language_token = NULL;
        for (size_t i = 0; i < x->lower->n; i++) {
                WKBranch *p = wk_array_get(x->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(p, XISyntax *);
                if (a->type == XI_SNIPPET_LANGUAGE) {
                        language_token = wk_link_get(a->tokens->head, XIToken *);
                        break;
                }
        }
        return language_token;
}
@

对于不存在语言标记的有名片段，为其构造语言标记节点的过程如下：

@ 构造语言标记节点 #
XIToken *new_lang_tok = malloc(sizeof(XIToken));
new_lang_tok->type = XI_TEXT;
new_lang_tok->content = wk_str(x_lang_tok->content->body);
/* 确定语言标记的行号 */
XISyntax *name = wk_branch_get(wk_array_get(x_bra->lower, 0, WKBranch *), XISyntax *);
XIToken *name_tok = wk_link_get(name->tokens->head, XIToken *);
size_t line_number = name_tok->line_number;
for (const char *p = name_tok->content->body; *p != '\0'; p++) {
        if (*p == '\n') line_number++;
}
new_lang_tok->line_number = line_number;
/* 将语言标记添加到语法树 */
XISyntax *new_lang = malloc(sizeof(XISyntax));
new_lang->type = XI_SNIPPET_LANGUAGE;
new_lang->tokens = wk_list(XIToken *);
wk_list_suffix(new_lang->tokens, new_lang_tok, XIToken *);
new_lang->id = 0;
/* 新节点会作为 x 的第 2 个节点 */
wk_tree_ins(xi_tree, x_bra, 1, new_lang, XISyntax *);
@

在语法树中所有的有名节点中传播语言标记的过程由辅助函数 \type{spread_language_mark} 实现：

@ 语言标记传播过程所需的辅助函数 # +
static void spread_language_mark(WKTree *xi_tree, WKTable *relations) {
        for (size_t i = 0; i < xi_tree->root->lower->n; i++) {
                WKBranch *x_bra = wk_array_get(xi_tree->root->lower, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type == XI_SNIPPET_WITH_NAME) {
                        XISyntax *y = wk_branch_get(wk_array_get(x_bra->lower,
                                                                 0,
                                                                 WKBranch *), XISyntax *);
                        XIToken *y_tok = wk_link_get(y->tokens->head, XIToken *);
                        WKList *thread = wk_list(WKBranch *);
                        get_thread(relations, y_tok->content, thread);
                        spread_language_mark_in_thread(xi_tree, thread);
                        wk_list_free(thread);
                }
        }
}
@

\subsection{直接输出}

设 xi 语法树第二层节点中任一节点 \type{x}，若为无名片段，则原样输出其内容，若为有名片段，则基于配置文件里的设定，格式化输出其内容。该过程以辅助函数 \type{xi_weave} 实现，如下

@ xi_weave # [C]
static void xi_weave(WKTree *xi_tree, WKTable *relations, FILE *output) {
        for (size_t i = 0; i < xi_tree->root->lower->n; i++) {
                WKBranch *x_bra = wk_array_get(xi_tree->root->lower, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type == XI_SNIPPET) { /* 无名片段，原样输出 */
                        XIToken *x_tok = wk_link_get(x->tokens->head, XIToken *);
                        fprintf(output, "%s", x_tok->content->body);
                } else {
                        output_snippet_with_name(xi_tree, x_bra, relations, output);
                }
        }
}
@

\noindent 上述代码中用于输出有名片段内容的辅助函数 \type{output_snippet_with_name}，其实现如下：

@ xi_weave # ^+
static void output_snippet_with_name(WKTree *syntax_tree, WKBranch *x_bra, WKTable *relations, FILE *output) {
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        WKStr *x_name = NULL;
        WKStr *x_id_text = wk_str(NULL);
        wk_str_printf(x_id_text, "%lu", x->id);
        # 输出片段开始标记 @
        for (size_t i = 0; i < x_bra->lower->n; i++) {
                WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
                XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                XIToken *y_tok = y->tokens ? wk_link_get(y->tokens->head, XIToken *) : NULL;
                switch (y->type) {
                case XI_SNIPPET_NAME:
                        # 输出片段名 @
                        break;
                case XI_SNIPPET_TAG_REFERENCE:
                        # 输出片段标签引用 @
                        break;
                case XI_SNIPPET_APPENDING_OPERATOR:
                        # 输出向后追加运算符 @
                        break;
                case XI_SNIPPET_PREPENDING_OPERATOR:
                        # 输出向前追加运算符 @
                        break;
                case XI_SNIPPET_TAG:
                        # 输出片段标签 @
                        break;
                case XI_SNIPPET_CONTENT:
                        # 输出片段内容 @
                        break;
                default: ;
                }
        }
        # 输出片段的引用者 @
        # 输出片段结束标记 @
        wk_str_free(x_id_text);
}
@

若用户提供了片段开始和结束标记，则将可能存在的占位符 \type{${language}} 替换为片段语言记号的内容，否则以片段界限符作为片段起始且忽略结束标记。

@ 输出片段开始标记 #
if (xi_fmt->snippet_start->n > 0) {
        XIToken *x_lang_tok = find_language_token(x_bra);
        WKStr *a = wk_str(xi_fmt->snippet_start->body);
        if (x_lang_tok) {
                wk_str_replace(a, "${language}", x_lang_tok->content->body);
        }
        fprintf(output, "%s", a->body);
        wk_str_free(a);
}
@

@ 输出片段结束标记 #
if (xi_fmt->snippet_stop->n > 0) {
        XIToken *x_lang_tok = find_language_token(x_bra);
        WKStr *a = wk_str(xi_fmt->snippet_stop->body);
        if (x_lang_tok) {
                wk_str_replace(a, "${language}", x_lang_tok->content->body);
        }
        fprintf(output, "%s", a->body);
        wk_str_free(a);
}
@

若用户提供了片段名标记，则将可能存在的占位符 \type{${name}} 和 \type{${id}} 替换为片段名及序号，否则只输出片段名以及片段名字界限符。片段名输出过程如下：

@ 输出片段名 #
x_name = y_tok->content; /* 保存片段名，留待输出片段引用者时用 */
if (xi_fmt->snippet_name->n > 0) {
        WKArray *segments = snippet_name_split(x_name);
        /* 输出片段定界符 */
        if (xi_fmt->snippet_name_start->n > 0) {
                fprintf(output, "%s", xi_fmt->snippet_name_start->body);
        } else {
                fprintf(output, "%s", xi_symbols->snippet_delimiter->body);
        }
        /* 输出名字 */
        for (size_t j = 0; j < segments->n; j++) {
                WKStr *segment = wk_array_get(segments, j, WKStr *);
                if (j % 2 == 0) {
                        WKStr *snippet_name_t = wk_str(xi_fmt->snippet_name->body);
                        wk_str_replace(snippet_name_t, "${name}", segment->body);
                        wk_str_replace(snippet_name_t, "${id}", x_id_text->body);
                        fprintf(output, "%s", snippet_name_t->body);
                        wk_str_free(snippet_name_t);
                } else { /* 输出续行符以及下一行的前导空白字符 */
                        if (xi_fmt->snippet_name_continuation->n > 0) {
                                wk_str_replace(segment,
                                               xi_symbols->snippet_name_continuation->body,
                                               xi_fmt->snippet_name_continuation->body);
                                fprintf(output, "%s", segment->body);
                        } else {
                                fprintf(output, "%s", segment->body);
                        }
                }
                wk_str_free(segment);
        }
        /* 输出名字定界符 */
        if (xi_fmt->snippet_name_stop->n > 0) {
                fprintf(output, "%s", xi_fmt->snippet_name_stop->body);
        } else {
                fprintf(output, "%s", xi_symbols->snippet_name_delimiter->body);
        }
        if (xi_fmt->snippet_id->n > 0) {
                WKStr *snippet_id = wk_str(xi_fmt->snippet_id->body);
                wk_str_replace(snippet_id, "${id}", x_id_text->body);
                fprintf(output, "%s", snippet_id->body);
                wk_str_free(snippet_id);
        }
        wk_array_free(segments);
} else fprintf(output, "%s%s%s",
                       xi_symbols->snippet_delimiter->body,
                       y_tok->content->body,
                       xi_symbols->snippet_name_delimiter->body);
@

\noindent 上述代码使用的 \type{snippet_name_split} 函数，是以续行符及其下一行的前导空白字符作为界限分割片段名，其定义如下：

@ xi_weave # ^+
WKArray *snippet_name_split(WKStr *name) {
        WKStr *continuation = xi_symbols->snippet_name_continuation;
        WKArray *segments = wk_array(WKStr *);
        WKStr *a = wk_str(NULL);
        WKStr *b = wk_str(NULL);
        const char *p = name->body;
        const char *q = name->body + name->n;
        WKStr *quad = wk_str("　");
        enum {INIT, MAYBE_WHITE_AREA, WHITE_AREA} state = INIT;
        while (1) {
                if (*p == '\0') break;
                switch (state) {
                case INIT:
                        if (here_is_me(p, q, p, 1, continuation)) {
                                p += continuation->n - 1;
                                wk_str_suffix(b, continuation->body);
                                state = MAYBE_WHITE_AREA;
                        } else {
                                wk_str_suffix_char(a, *p);
                        }
                        break;
                case MAYBE_WHITE_AREA:
                        if (*p == ' ' || *p == '\t') {
                                wk_str_suffix_char(b, *p);
                        } else if (here_is_me(p, q, p, 1, quad)) {
                                wk_str_suffix(b, quad->body);
                        } else if (*p == '\n') {
                                wk_str_suffix_char(b, *p);
                                wk_array_add(segments, a, WKStr *);
                                a = wk_str(NULL);
                                state = WHITE_AREA;
                        } else {
                                wk_str_suffix_char(b, *p);
                                wk_str_suffix(a, b->body);
                                b = wk_str(NULL);
                                state = INIT;
                        }
                        break;
                case WHITE_AREA:
                        if (*p == ' ' || *p == '\t') {
                                wk_str_suffix_char(b, *p);
                        } else if (here_is_me(p, q, p, 1, quad)) {
                                wk_str_suffix(b, quad->body);
                        } else {
                                wk_str_suffix_char(a, *p);
                                wk_array_add(segments, b, WKStr *);
                                b = wk_str(NULL);
                                state = INIT;
                        }
                        break;
                default:
                        fprintf(stderr, "Illegal state in <%s>!\n", name->body);
                        exit(EXIT_FAILURE);
                }
                p++;
        }
        if (b->n > 0) wk_str_suffix(a, b->body);
        wk_str_free(b);
        if (a->n > 0) wk_array_add(segments, a, WKStr *);
        else wk_str_free(a);
        wk_str_free(quad);
        return segments;
}
@

若用户提供了片段标签标记，则需要替换 \type{${id}} 和 \type{${name}}，否则输出默认的标签引用形式：

@ 输出片段标签引用 #
if (xi_fmt->snippet_tag_reference->n > 0) {
        WKStr *a = wk_str(xi_fmt->snippet_tag_reference->body);
        wk_str_replace(a, "${id}", x_id_text->body);
        wk_str_replace(a, "${name}", y_tok->content->body);
        fprintf(output, "%s", a->body);
        wk_str_free(a);
} else {
        fprintf(output, " %s%s%s",
                        xi_symbols->tag_start_mark->body,
                        y_tok->content->body,
                        xi_symbols->tag_stop_mark->body);
}
@

向后与向前追加运算符标记可由用户定义，也可使用 xi 默认符号：

@ 输出向后追加运算符 #
if (xi_fmt->snippet_appending_operator->n > 0) {
        fprintf(output, "%s", xi_fmt->snippet_appending_operator->body);
} else fprintf(output, " %s", xi_symbols->snippet_appending_mark->body);
@

@ 输出向前追加运算符 #
if (xi_fmt->snippet_prepending_operator->n > 0) {
        fprintf(output, "%s", xi_fmt->snippet_prepending_operator->body);
} else fprintf(output, " %s", xi_symbols->snippet_prepending_mark->body);
@

片段标签格式化过程与其引用形式近乎相同：

@ 输出片段标签 #
do {
        XIToken *tag_tok = wk_link_get(y->tokens->head, XIToken *);
        if (xi_fmt->snippet_tag->n > 0) {
                WKStr *a = wk_str(xi_fmt->snippet_tag->body);
                wk_str_replace(a, "${id}", x_id_text->body);
                wk_str_replace(a, "${name}", tag_tok->content->body);
                fprintf(output, "%s", a->body);
                wk_str_free(a);
        } else {
                fprintf(output, "\n%s%s%s",
                                xi_symbols->tag_start_mark->body,
                                tag_tok->content->body,
                                xi_symbols->tag_stop_mark->body);
        }
} while (0);
@

片段的内容是在 \type{x} 的下层节点里，且只有两种类型 \type{XI_TEXT} 与 \type{XI_SNIPPET_REFERENCE}，其输出过程如下：

@ 输出片段内容 #
for (size_t j = 0; j < y_bra->lower->n; j++) {
        WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
        XISyntax *z = wk_branch_get(z_bra, XISyntax *);
        XIToken *z_tok = wk_link_get(z->tokens->head, XIToken *);
        if (z->type == XI_SNIPPET_TEXT) {
                fprintf(output, "%s", z_tok->content->body);
        } else {
                WKStr *key = compact_text(z_tok->content, xi_skipped_chars_in_name);
                WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
                wk_str_free(key);
                if (tie_box) {
                        # 输出片段引用 @
                } else {
                        fprintf(stderr, "Line %lu: the snippet <%s> not defined!\n",
                                        z_tok->line_number, z_tok->content->body);
                }
        }
}
@

上述代码中的片段引用部分的输出代码如下：

@ 输出片段引用 #
XITie *tie = wk_box_get(tie_box, XITie *);
if (xi_fmt->snippet_reference->n > 0) {
        WKArray *segments = snippet_name_split(z_tok->content);
        WKStr *z_id_text = wk_str(NULL);
        WKBranch *a_bra = wk_array_get(tie->spatial_order, 0, WKBranch *);
        XISyntax *a = wk_branch_get(a_bra, XISyntax *);
        wk_str_printf(z_id_text, "%lu", a->id);
        if (xi_fmt->snippet_reference_start->n > 0) {
                fprintf(output, "%s", xi_fmt->snippet_reference_start->body);
        } else {
                fprintf(output, "%s", xi_symbols->snippet_reference_start_mark->body);
        }
        for (size_t k = 0; k < segments->n; k++) {
                WKStr *segment = wk_array_get(segments, k, WKStr *);
                if (k % 2 == 0) {
                        WKStr *snippet_name_t = wk_str(xi_fmt->snippet_reference->body);
                        wk_str_replace(snippet_name_t, "${name}", segment->body);
                        wk_str_replace(snippet_name_t, "${id}", z_id_text->body);
                        fprintf(output, "%s", snippet_name_t->body);
                        wk_str_free(snippet_name_t);
                } else { /* 输出续行符以及下一行的前导空白字符 */
                        if (xi_fmt->snippet_name_continuation->n > 0) {
                                wk_str_replace(segment,
                                               xi_symbols->snippet_name_continuation->body,
                                               xi_fmt->snippet_name_continuation->body);
                                fprintf(output, "%s", segment->body);
                        } else {
                                fprintf(output, "%s", segment->body);
                        }
                }
                wk_str_free(segment);
        }
        wk_array_free(segments);
        if (xi_fmt->snippet_reference_stop->n > 0) {
                fprintf(output, "%s", xi_fmt->snippet_reference_stop->body);
        } else {
                fprintf(output, "%s", xi_symbols->snippet_reference_stop_mark->body);
        }
        /* 输出片段 id 序列，若是同名片段，只输出首个 id */
        if (xi_fmt->snippet_reference_id->n > 0) {
                WKStr *z_id_fmt = wk_str(xi_fmt->snippet_reference_id->body);
                wk_str_replace(z_id_fmt, "${id}", z_id_text->body);
                fprintf(output, "%s", z_id_fmt->body);
                wk_str_free(z_id_fmt);
        }
        wk_str_free(z_id_text);
} else {
        fprintf(output, "%s%s%s",
                         xi_symbols->snippet_reference_start_mark->body,
                         z_tok->content->body,
                         xi_symbols->snippet_reference_stop_mark->body);
}
@

如果片段 \type{x} 存在引用者，则将其格式化输出，过程如下：

@ 输出片段的引用者 #
if (xi_fmt->snippet_emission->n > 0) {
        WKStr *x_key = compact_text(x_name, xi_skipped_chars_in_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(x_key, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);
                if (tie->emissions->n > 0
                    && x_bra == wk_array_get(tie->spatial_order, 0, WKBranch *)) {
                        WKStr *y_id_text = wk_str(NULL);
                        for (size_t i = 0; i < tie->emissions->n; i++) {
                                WKBranch *y_bra = wk_array_get(tie->emissions, i, WKBranch *);
                                XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                                WKBranch *y_name_bra = wk_array_get(y_bra->lower, 0, WKBranch *);
                                XISyntax *y_name = wk_branch_get(y_name_bra, XISyntax *);
                                XIToken *y_name_tok = wk_link_get(y_name->tokens->head, XIToken *);
                                wk_str_printf(y_id_text, "%lu", y->id);
                                WKStr *y_fmt = wk_str(xi_fmt->snippet_emission->body);
                                wk_str_replace(y_fmt, "${name}", y_name_tok->content->body);
                                wk_str_replace(y_fmt, "${id}", y_id_text->body);
                                fprintf(output, "%s", y_fmt->body);
                                wk_str_free(y_fmt);
                        }
                        wk_str_free(y_id_text);
                }
        }
        wk_str_free(x_key);
}
@

\subsection{YAML 输出}

在 \type{xi_weave} 实现的片段格式化输出过程中，用户只能通过 xi 的配置文件予以干预，故而无法实现程序源码高亮渲染、特殊符号替换等更为精细的格式化处理。我选择以 YAML 格式作为 xi 的另一种输出格式，用户可基于 xi 输出的 YAML 格式的文档编写后端程序，从而实现他们所期望的格式化处理。

无名片段输出的 YAML 数据项的形式为

\starttyping
- type: no-name
  content: 无名片段的内容
\stoptyping

有名片段的 YAML 数据项颇为复杂，形如

\starttyping
- type: with-name
  name: 片段名
  id: 编号（从 1 开始的整数），具有唯一性
  language: 语言
  tag_reference: 标签引用
  operator: 片段运算符
  tag: 标签
  content:
    - type: text
      data: 普通内容
    - type: reference
      data:
        name: 被引用片段的名字
        id: 被引用片段的编号
    - type: text
      data: ...
    - type: reference
      data:
        ... ... ...
  emissions: # 若该片段存在引用者，则该项为引用者序列，否则该项不存在
    - name: 引用者的名字
      id: 引用者的编号
    - name: ...
      ... ... ...
  - ...
\stoptyping

输出 YAML 格式数据的函数是 \type{xi_yaml_output}，其定义如下：

@ xi_yaml_output # [C]
static void xi_yaml_output(WKTree *xi_tree,
                           WKTable *relations,
                           XIFmt *fmt,
                           FILE *output) {
        for (size_t i = 0; i < xi_tree->root->lower->n; i++) {
                WKBranch *x_bra = wk_array_get(xi_tree->root->lower, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type == XI_SNIPPET) { /* 无名片段，原样输出 */
                        XIToken *x_tok = wk_link_get(x->tokens->head, XIToken *);
                        fprintf(output, "- type: no-name\n"
                                        "  content: |-\n"
                                        "    '");
                        yaml_output_multilines(output, x_tok->content->body, "    ");
                        fprintf(output, "'\n");
                } else {
                        snippet_with_name_to_yaml(xi_tree, x_bra, relations, fmt, output);
                }
        }
}
@

\noindent 上述代码以 YAML 格式输出无名片段内容时，若内容存在多行文本，则每次换行时，下一行的行首必须流出足够的缩进，否则无法构成合法的 YAML 跨行文本。辅助函数 \type{yaml_output_multilines} 专用于输出符合 YAML 格式的跨行文本，其定义如下：

@ xi_yaml_output # ^+
<yaml-output-multilines>
static void yaml_output_multilines(FILE *output, const char *text, const char *indent) {
        WKStr *cache = wk_str(NULL);
        for (const char *p = text; *p != '\0'; p++) {
                wk_str_suffix_char(cache, *p);
                if (*p == '\n') {
                        fprintf(output, "%s%s", cache->body, indent);
                        wk_str_free(cache);
                        cache = wk_str(NULL);
                }
        }
        if (cache->n > 0) fprintf(output, "%s", cache->body);
        wk_str_free(cache);
}
@

在 \type{xi_yaml_output} 定义中所用的辅助函数 \type{snippet_with_name_to_yaml} 可格式化输出有名片段的内容，其定义如下：

@ xi_yaml_output # <yaml-output-multilines> +
static void snippet_with_name_to_yaml(WKTree *syntax_tree,
                                      WKBranch *x_bra,
                                      WKTable *relations,
                                      XIFmt *fmt,
                                      FILE *output) {
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        WKStr *x_name = NULL;
        WKStr *x_id_text = wk_str(NULL);
        fprintf(output, "- type: with-name\n");
        for (size_t i = 0; i < x_bra->lower->n; i++) {
                WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
                XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                XIToken *y_tok = y->tokens ? wk_link_get(y->tokens->head, XIToken *) : NULL;
                switch (y->type) {
                case XI_SNIPPET_NAME:
                        x_name = y_tok->content;
                        fprintf(output, "  name: |-\n"
                                        "    '");
                        yaml_output_multilines(output, x_name->body, "    ");
                        fprintf(output, "'\n"
                                        "  id: %lu\n", x->id);
                        break;
                case XI_SNIPPET_LANGUAGE:
                        fprintf(output, "  language: %s\n", y_tok->content->body);
                        break;
                case XI_SNIPPET_TAG_REFERENCE:
                        fprintf(output, "  tag_reference: '%s'\n", y_tok->content->body);
                        break;
                case XI_SNIPPET_APPENDING_OPERATOR:
                        fprintf(output, "  operator: %s\n", y_tok->content->body);
                        break;
                case XI_SNIPPET_PREPENDING_OPERATOR:
                        fprintf(output, "  operator: %s\n", y_tok->content->body);
                        break;
                case XI_SNIPPET_TAG:
                        fprintf(output, "  tag: '%s'\n", y_tok->content->body);
                        break;
                case XI_SNIPPET_CONTENT:
                        # YAML 格式化：片段内容 @
                        break;
                default: ;
                }
        }
        # YAML 格式化：片段的引用者 @
        wk_str_free(x_id_text);
}
@

\noindent 上述代码中，片段内容的 YAML 格式输出过程如下：

@ YAML 格式化：片段内容 #
fprintf(output, "  content:\n");
for (size_t j = 0; j < y_bra->lower->n; j++) {
        WKBranch *z_bra = wk_array_get(y_bra->lower, j, WKBranch *);
        XISyntax *z = wk_branch_get(z_bra, XISyntax *);
        XIToken *z_tok = wk_link_get(z->tokens->head, XIToken *);
        if (z->type == XI_SNIPPET_TEXT) {
                fprintf(output, "    - type: text\n"
                                "      data: |-\n"
                                "        '");
                yaml_output_multilines(output, z_tok->content->body, "        ");
                fprintf(output, "'\n");
        } else if (z->type == XI_SNIPPET_REFERENCE) {
                WKStr *key = compact_text(z_tok->content, xi_skipped_chars_in_name);
                WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
                fprintf(output, "    - type: snippet-reference\n");
                if (tie_box) {
                        XITie *tie = wk_box_get(tie_box, XITie *);
                        fprintf(output, "      name: |-\n"
                                        "        '");
                        yaml_output_multilines(output, z_tok->content->body, "        ");
                        fprintf(output, "'\n"
                                        "      ids:\n");
                        /* 输出所引用的片段 id，同名片段只输出首个片段 id */
                        WKBranch *z_ref_bra = wk_array_get(tie->spatial_order, 0, WKBranch *);
                        XISyntax *z_ref = wk_branch_get(z_ref_bra, XISyntax *);
                        fprintf(output, "        - %lu\n", z_ref->id);
                } else {
                        fprintf(stderr, "Line %lu: the snippet <%s> not defined!\n",
                                        z_tok->line_number, z_tok->content->body);

                }
                wk_str_free(key);
        } else ;
}
@

上述辅助函数 \type{snippet_with_name_to_yaml} 中的片段引用者集合的格式化过程如下：

@ YAML 格式化：片段的引用者 #
WKStr *x_key = compact_text(x_name, xi_skipped_chars_in_name);
WKBox *tie_box = wk_table_query(relations, wk_box_ref(x_key, WKStr *));
if (tie_box) {
        XITie *tie = wk_box_get(tie_box, XITie *);
        if (tie->emissions->n > 0
            && x_bra == wk_array_get(tie->spatial_order, 0, WKBranch *)) {
                fprintf(output, "  emissions:\n");
                for (size_t i = 0; i < tie->emissions->n; i++) {
                        WKBranch *y_bra = wk_array_get(tie->emissions, i, WKBranch *);
                        XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                        WKBranch *y_name_bra = wk_array_get(y_bra->lower, 0, WKBranch *);
                        XISyntax *y_name = wk_branch_get(y_name_bra, XISyntax *);
                        XIToken *y_name_tok = wk_link_get(y_name->tokens->head, XIToken *);
                        fprintf(output, "    - name: |-\n"
                                        "        '%s'\n"
                                        "      id: %lu\n", y_name_tok->content->body, y->id);
                }
        }
}
wk_str_free(x_key);
@

\subsection{小结}

将格式化输出函数汇入 xi 的辅助函数集：

@ xi 的数据结构与辅助函数 # +
# 语言标记传播过程所需的辅助函数 @
# xi_weave @
# xi_yaml_output @
@

\section{界面}

xi 的最终形式是一个命令行程序，拥有以下命令选项：

\startitemize
\item \type{--tangle} 或 \type{-t}，用于抽取代码片段；
\item \type{--entrance} 或 \type{-e}，与 \type{--tangle} 配合使用，用于指定待提取代码片段的名称；
\item \type{--line} 或 \type{-l}，用于开启所提取代码片段在文学程序中对应的行号；
\item \type{--weave} 或 \type{-w}，用于构建文学程序排版文档；
\item \type{--yaml} 或 \type{-y}，用于开启 xi 的 YAML 文档输出模式;
\item \type{--config} 或 \type{-c}，用于指定 xi 配置文件的路径；
\item \type{--output} 或 \type{-o}，用于设定 xi 的输出文件名称或其路径；
\item \type{--help} 或 \type{-h}，帮助选项，输出各选项的功能。
\stopitemize

可使用 WK 库的 WKCfg 模块解析这些命令选项。首先载入该模块的头文件：

@ 外境 # +
#include <wk-cli.h>
@

然后假设 xi 的 main 函数的参数为 \type{argc} 与 \type{argv}，以下代码构造了全部选项变量：

@ 基于 main 函数的参数 argc 与 argc 构造 xi 选项 #
WKArg xi_args[] = {
        WK_ARG_TOGGLE("tangle", "t"),
        WK_ARG_TEXT("entrance", "e"),
        WK_ARG_TOGGLE("line", "l"),
        WK_ARG_TOGGLE("weave", "w"),
        WK_ARG_TOGGLE("yaml", "y"),
        WK_ARG_TEXT("config", "c"),
        WK_ARG_TEXT("output", "o"),
        WK_ARG_TOGGLE("help", "h"),
        WK_ARG_TEXT(NULL, NULL) /* 无选项参数：文学程序文件名 */
};
size_t n_of_opts = sizeof(xi_args) / sizeof(WKArg);
wk_cli_parse(argv, argc, xi_args, n_of_opts);
/* 选项变量 */
bool tangle_opt = xi_args[0].value.toggle;
const char *entrance_opt = xi_args[1].value.text;
if (strcmp(entrance_opt, "") == 0) entrance_opt = NULL;
bool line_opt = xi_args[2].value.toggle;
bool weave_opt = xi_args[3].value.toggle;
bool yaml_opt = xi_args[4].value.toggle;
const char *xi_config_path = xi_args[5].value.text;
if (strcmp(xi_config_path, "") == 0) xi_config_path = NULL;
const char *output_opt = xi_args[6].value.text;
if (strcmp(output_opt, "") == 0) output_opt = NULL;
bool help_opt = xi_args[7].value.toggle;
const char *xi_file_path = xi_args[8].value.text;
if (strcmp(xi_file_path, "") == 0) xi_file_path = NULL;
/* 选项合法性检测 */
if (tangle_opt && weave_opt) {
        fprintf(stderr,
                "--tangle and --weave can not be used simultaneously!\n");
        exit(EXIT_FAILURE);
} else {
        if (tangle_opt) {
                if (!entrance_opt) {
                        fprintf(stderr, "You should provide entrances!\n");
                        exit(EXIT_FAILURE);
                }
        }
        if (weave_opt) {
                if (!output_opt) {
                        fprintf(stderr, "You should provide an output path!\n");
                        exit(EXIT_FAILURE);
                }
        }
}
@

在 \type{--help} 模式下，仅输出 xi 的帮助信息：

@ 处理 --help 模式 #
fprintf(stdout, "COMMAND: xi [OPTION]... FILE\n\n");
fprintf(stdout, "%-16s  %s\n", "--tangle, -t",   "extract code snippets.");
fprintf(stdout, "%-16s  %s\n", "--entrance, -e", "specify the name of the code fragment to extract.");
fprintf(stdout, "%-16s  %s\n", "--line, -l",     "enable line numbers corresponding to the extracted");
fprintf(stdout, "%-16s  %s\n", "",                "code fragment in the literate program.");
fprintf(stdout, "%-16s  %s\n", "--weave, -w",     "build the typeset documentation of the literate");
fprintf(stdout, "%-16s  %s\n", "",                "program.");
fprintf(stdout, "%-16s  %s\n", "--yaml, -y",      "enable yaml output.");
fprintf(stdout, "%-16s  %s\n", "--config, -c",   "specify the path to the xi configuration file.");
fprintf(stdout, "%-16s  %s\n", "--output, -o",   "set the output file name or path for xi.");
fprintf(stdout, "%-16s  %s\n", "--help, -h",     "display this help and exit.");
@

在 \type{--tangle} 模式下，\type{--entrance} 和 \type{--output} 的值可能是多个，以西文逗号间隔，需将其解析为两个元素个数相同的数组：

@ 解析选项 --entrance 和 --output 的值 #
const char *opt_sep = ",";
WKArray *entrances = str_split(entrance_opt, opt_sep);
WKArray *outputs = str_split(output_opt, opt_sep);
if (outputs->n == 0) {
        for (size_t i = 0; i < entrances->n; i++) {
                WKStr *a = wk_array_get(entrances, i, WKStr *);
                WKStr *b = wk_str(a->body);
                wk_array_add(outputs, b, WKStr *);
        }
}
if (entrances->n != outputs->n) {
        fprintf(stderr, "Illegal output!\n");
        exit(EXIT_FAILURE);
}
@

\noindent 上述代码中用于分割字符串的函数 \type{str_split}，其定义如下

@ xi 的数据结构与辅助函数 # +
WKArray *str_split(const char *str, const char *sep) {
        /* 主动在 str 的内容尾部追加 sep，从而将分割过程限制在一个循环里 */
        WKStr *str_1 = wk_str(str);
        wk_str_suffix(str_1, sep);
        /* 根据 sep 分割 */
        WKArray *a = wk_array(WKStr *);
        const char *p = str_1->body;
        const char *q = p;
        size_t n = strlen(sep);
        while (1) {
                q = strstr(q, sep);
                if (q) {
                        WKStr *s = wk_str(NULL);
                        for (const char *r = p; r != q; r++) {
                                wk_str_suffix_char(s, *r);
                        }
                        if (s->n > 0) {
                                wk_str_trim(s);
                                wk_array_add(a, s, WKStr *);
                        } else wk_str_free(s);
                        q += n;
                        p = q;
                } else break;
        }
        wk_str_free(str_1);
        return a;
}
@

以下代码解析配置文件，构造符号表，读入文学程序文件并完成词法和语法分析：

@ xi 核心过程 #
# 符号表初始化 @
# 配置文件解析 @
# 打开文学程序文件 -> xi_file @
WKList *xi_tokens = xi_lexer(xi_file);
WKTree *xi_tree = xi_parser(xi_tokens);
# 构造冗白字符集 xi_skipped_chars_in_name @
# 为关系表注册 XITie 对象释放函数 @
WKTable *relations = xi_create_relations(xi_tree->root);
@

上述过程分配的资源，需在 xi 进程退出前释放：

@ xi 进程资源回收 # +
wk_table_free(relations);
xi_delete_syntax_tree(xi_tree);
@

若 \type{--tangle} 模式开启，则调用 \type{xi_tangle} 抽取 \type{entrances} 中给定的代码片段，并输出到 \type{outputs} 中相应下标对应的输出端。

@ 处理 --tangle 模式 #
# 解析选项 --entrance 和 --output 的值 @ /* 得到 entrances 与 outputs */
WKList *indents = wk_list(WKStr *);
for (size_t i = 0; i < entrances->n; i++) {
        WKStr *entrance = wk_array_get(entrances, i, WKStr *);
        WKStr *output_path = wk_array_get(outputs, i, WKStr *);
        FILE *output = fopen(output_path->body, "w");
        if (!output) {
                fprintf(stderr, "Failed to open %s!\n", output_path->body);
                exit(EXIT_FAILURE);
        }
        WKStr *cache = wk_str(NULL);
        xi_tangle(xi_file_path, xi_tree, relations,
                  entrance, 0, indents, line_opt, cache);
        fputs(cache->body, output);
        wk_str_free(cache);
        fclose(output);
}
for (WKLink *it = indents->head; it; it = it->next) {
        WKStr *indent = wk_link_get(it, WKStr *);
        wk_str_free(indent);
}
wk_list_free(indents);
for (size_t i = 0; i < entrances->n; i++) {
        wk_str_free(wk_array_get(entrances, i, WKStr *));
        wk_str_free(wk_array_get(outputs, i, WKStr *));
}
wk_array_free(entrances);
wk_array_free(outputs);
@

在 \type{--weave} 模式下，\type{output_opt} 表示单个文件的名字或路径，通过它构造文学程序的文档格式输出，具体过程如下：

@ 处理 --weave 模式 #
FILE *xi_output = fopen(output_opt, "w");
spread_language_mark(xi_tree, relations);
if (yaml_opt) {
        xi_yaml_output(xi_tree, relations, xi_fmt, xi_output);
} else xi_weave(xi_tree, relations, xi_output);
fclose(xi_output);
@

将上述代码片段组装为 xi 程序：

@ xi 程序 # [C]
# 外境 @
# 全局变量 @
# xi 的数据结构与辅助函数 @
int main(int argc, char **argv) {
        # 基于 main 函数的参数 argc 与 argc 构造 xi 选项 @
        if (help_opt) {
                # 处理 --help 模式 @
        } else {
                # xi 核心过程 @
                if (tangle_opt) {
                        # 处理 --tangle 模式 @
                }
                if (weave_opt) {
                        # 处理 --weave 模式 @
                }
                # xi 进程资源回收 @
        }
        return 0;
}
@

\appendix{后记}

断断续续，每天无聊到发慌时，便写几行，历时两月有余，orez 变成了 xi。原先，我对 orez 不满之处，现在皆已填平，可心里又有了新的不满。

xi 程序本身是以它所支持的文学编程形式编写的。这个程序写得并不太好。我的本意是希望它能蕴含一些文学性，可是每当需要引出一些代码片段时，我会不由自主地陷入这样一种叙述方式，\type{xxxx 的实现如下……}。这不叫文学，这叫说明书吧……应该有更好的叙事方式，只是自古至今，没人去思考该如何去叙述一个程序。

也许更好的叙事方式是，自然语言部分不应当总是陷入为程序语言服务的角色，而是以文学的方式描述程序片段的一些意象，如此未必不能将一个程序写成小说。当然，这只是我的猜测，而我现在并无能力和足够的时间去做一些尝试了。

\stoptext
