#line 3131 "xi.xi"
#line 266 "xi.xi"
#include <wk-cfg.h>
#line 1170 "xi.xi"
#include <wk-tree.h>
#line 2943 "xi.xi"
#include <wk-cli.h>
#line 3132 "xi.xi"
#line 402 "xi.xi"
WKArray *xi_skipped_chars; /* xi 空白字符集 */
#line 1638 "xi.xi"
WKArray *xi_skipped_chars_in_name;
#line 3133 "xi.xi"
#line 1059 "xi.xi"
#line 199 "xi.xi"
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
#line 189 "xi.xi"
typedef struct {
        XITokenType type;
        size_t line_number;
        WKStr *content;
} XIToken;
#line 219 "xi.xi"
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
#line 294 "xi.xi"
#define XI_USER_CONFIG(user_config, xi_symbols, item) do { \
        WKBox *v = wk_table_query(user_config, wk_box_ref(#item, const char *)); \
	if (v) { \
                if (xi_symbols->item) wk_str_free(xi_symbols->item); \
                xi_symbols->item = wk_str(wk_box_get(v, WKStr *)->body); \
        } \
} while (0)
#line 2138 "xi.xi"
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
#line 1060 "xi.xi"
#line 541 "xi.xi"
static bool here_is_me(const char *a, const char *b,
                       const char *here, int dir, WKStr *me) {
        size_t n = me->n;
        const char *p = dir < 0 ? (here - n + 1) : here;
        if (p < a) return false;
        if (p + n > b) return false;
        return memcmp(me->body, p, n) == 0 ? true : false;
}
#line 377 "xi.xi"
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
#line 344 "xi.xi"
static int read_char(FILE *xi_file, WKStr *cache) {
        int c = fgetc(xi_file);
        if (c != EOF) {
                wk_str_suffix_char(cache, (char)c);
                return c;
        } else return EOF;
}
#line 360 "xi.xi"
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
#line 431 "xi.xi"
static void add_snippet(WKList *tokens, WKStr *cache) {
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_TEXT;
#line 444 "xi.xi"
        size_t n;
        WKLink *tail = tokens->tail;
        if (tail) {
                XIToken *last_token = wk_link_get(tail, XIToken *);
                n = last_token->line_number;
                for (const char *p = last_token->content->body; *p != '\0'; p++) {
                        if (*p == '\n') n++;
                }
        } else  n = 1;
#line 435 "xi.xi"
        snippet->line_number = n;
        snippet->content = cache;
        wk_list_suffix(tokens, snippet, XIToken *);
}
#line 458 "xi.xi"
static void add_snippet_delimiter(WKList *tokens) {
        WKStr *snippet_delimiter = xi_symbols->snippet_delimiter;
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_SNIPPET_DELIMITER;
#line 444 "xi.xi"
        size_t n;
        WKLink *tail = tokens->tail;
        if (tail) {
                XIToken *last_token = wk_link_get(tail, XIToken *);
                n = last_token->line_number;
                for (const char *p = last_token->content->body; *p != '\0'; p++) {
                        if (*p == '\n') n++;
                }
        } else  n = 1;
#line 463 "xi.xi"
        snippet->line_number = n;
        snippet->content = wk_str(snippet_delimiter->body);
        wk_list_suffix(tokens, snippet, XIToken *);
}
#line 501 "xi.xi"
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
#line 654 "xi.xi"
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
                        } else if (*p == '\n') state = FAILURE;
                        else state = MAYBE_MARK;
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
#line 623 "xi.xi"
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
#line 711 "xi.xi"
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
#line 801 "xi.xi"
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
#line 836 "xi.xi"
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
#line 907 "xi.xi"
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
#line 960 "xi.xi"
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
#line 1061 "xi.xi"
#line 1076 "xi.xi"
static void delete_tokens(WKList *tokens) {
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                wk_str_free(t->content);
                free(t);
        }
        wk_list_free(tokens);
}
#line 1062 "xi.xi"
static WKList *xi_lexer(FILE *xi_file) {
#line 406 "xi.xi"
        xi_skipped_chars = wk_array(WKStr *);
        wk_array_add(xi_skipped_chars, wk_str(" "), WKStr *);
        wk_array_add(xi_skipped_chars, wk_str("\t"), WKStr *);
        wk_array_add(xi_skipped_chars, wk_str("　"), WKStr *);
#line 1064 "xi.xi"
#line 472 "xi.xi"
        WKList *xi_tokens = wk_list(XIToken *);
        WKStr *cache = wk_str(NULL);
        while (1) {
                int status = read_char(xi_file, cache);
                if (status == EOF) break;
                else {
                        bool t = tail_is_snippet_delimiter(cache);
                        if (t) {
#line 423 "xi.xi"
                                size_t sni_del_length = xi_symbols->snippet_delimiter->n;
                                size_t sni_del_begin = cache->n - sni_del_length;
                                wk_str_del(cache, sni_del_begin, sni_del_length);
#line 481 "xi.xi"
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
#line 1065 "xi.xi"
#line 556 "xi.xi"
        WKLink *it = xi_tokens->head;
        while (it) {
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_TEXT) {
                        const char *p = find_name_delimiter(t);
                        if (p) {
#line 590 "xi.xi"
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
#line 564 "xi.xi"
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
#line 1066 "xi.xi"
#line 759 "xi.xi"
        it = xi_tokens->head;
        while (1) {
                if (!it) break;
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_TEXT) {
                        WKLink *prev = it->prev;
                        if (prev) {
                                XIToken *a = wk_link_get(prev, XIToken *);
                                if (a->type == XI_SNIPPET_NAME_DELIMITER) {
#line 744 "xi.xi"
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
#line 769 "xi.xi"
                                }
                        }
                }
                it = it->next;
        }
#line 1067 "xi.xi"
#line 873 "xi.xi"
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
#line 783 "xi.xi"
                                        printf("|%s|\n", xi_symbols->tag_start_mark->body);
                                        printf("|%s|\n", xi_symbols->tag_stop_mark->body);
                                        WKStr *tag_reference = extract_block(t, xi_symbols->tag_start_mark, xi_symbols->tag_stop_mark);
                                        if (tag_reference) {
                                                printf("|%s|\n", tag_reference->body);
                                                add_block(xi_tokens,
                                                          it,
                                                          tag_reference,
                                                          xi_symbols->tag_start_mark,
                                                          XI_TAG_START_MARK,
                                                          xi_symbols->tag_stop_mark,
                                                          XI_TAG_STOP_MARK);
                                        }
#line 884 "xi.xi"
#line 852 "xi.xi"
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
#line 885 "xi.xi"
                                }
                        }
                }
                it = it->next;
        }
#line 1068 "xi.xi"
#line 1020 "xi.xi"
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
#line 1044 "xi.xi"
                                                WKPair *snippet_reference = find_snippet_reference(t->content);
                                                if (!snippet_reference) break;
                                                else {
                                                        split_snippet(xi_tokens, it, snippet_reference);
                                                        wk_box_free(snippet_reference->x);
                                                        wk_box_free(snippet_reference->y);
                                                        free(snippet_reference);
                                                }
#line 1033 "xi.xi"
                                        }
                                }
                        }
                }
                it = it->next;
        }
#line 1069 "xi.xi"
        return xi_tokens;
}
#line 1593 "xi.xi"
#line 1150 "xi.xi"
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
#line 1140 "xi.xi"
typedef struct {
        XISyntaxType type;
        WKList *tokens;
        size_t id;
} XISyntax;
#line 1594 "xi.xi"
#line 1180 "xi.xi"
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
#line 1219 "xi.xi"
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
#line 1194 "xi.xi"
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
#line 1389 "xi.xi"
static void add_branches_for_snippet_with_name(WKTree *syntax_tree, WKBranch *x_bra) {
#line 1243 "xi.xi"
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        WKLink *it = x->tokens->head;
        XIToken *x_tok = wk_link_get(it, XIToken *);
#line 1251 "xi.xi"
        XISyntax *x_name = malloc(sizeof(XISyntax));
        x_name->type = XI_SNIPPET_NAME;
        x_name->tokens = wk_list(XIToken *);
        x_name->id = 0;
        wk_list_suffix(x_name->tokens, x_tok, XIToken *);
        wk_tree_add(syntax_tree, x_bra, x_name, XISyntax *);
#line 1262 "xi.xi"
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
#line 1282 "xi.xi"
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
#line 1311 "xi.xi"
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
#line 1334 "xi.xi"
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
#line 1352 "xi.xi"
        XISyntax *x_content = malloc(sizeof(XISyntax));
        x_content->type = XI_SNIPPET_CONTENT;
        x_content->tokens = wk_list(XIToken *);
        x_content->id = 0;
        for (WKLink *it_a = it; it_a; it_a = it_a->next) {
                x_tok = wk_link_get(it_a, XIToken *);
                wk_list_suffix(x_content->tokens, x_tok, XIToken *);
        }
        wk_tree_add(syntax_tree, x_bra, x_content, XISyntax *);
#line 1391 "xi.xi"
#line 1366 "xi.xi"
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
#line 1392 "xi.xi"
#line 1382 "xi.xi"
        wk_list_free(x->tokens);
        x->tokens = NULL;
#line 1393 "xi.xi"
}
#line 1421 "xi.xi"
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
#line 1595 "xi.xi"
#line 1487 "xi.xi"
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
#line 1596 "xi.xi"
/* #line 1509 "xi.xi"
#line 1090 "xi.xi"
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
#line 1510 "xi.xi"
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
#line 1596 "xi.xi" */
#line 1569 "xi.xi"
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
#line 1598 "xi.xi" 
static WKTree *xi_parser(WKList *tokens) {
        WKTree *xi_tree = syntax_tree_init(tokens);
#line 1399 "xi.xi"
        WKArray *branches = xi_tree->root->lower;
        for (size_t i = 0; i < branches->n; i++) {
                WKBranch *x_bra = wk_array_get(branches, i, WKBranch *);
                XISyntax *x = wk_branch_get(x_bra, XISyntax *);
                if (x->type == XI_SNIPPET_WITH_NAME) {
                        add_branches_for_snippet_with_name(xi_tree, x_bra);
                }
        }
#line 1601 "xi.xi"
#line 1465 "xi.xi"
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
#line 1602 "xi.xi"
        return xi_tree;
}
#line 1923 "xi.xi"
#line 1611 "xi.xi"
typedef struct {
        WKArray *time_order;
        WKArray *spatial_order;
        WKArray *emissions;
} XITie;
#line 1924 "xi.xi"
#line 1625 "xi.xi"
static WKStr *compact_text(WKStr *a, WKArray *skipped_chars) {
        WKStr *b = wk_str(a->body);
        for (size_t i = 0; i < skipped_chars->n; i++) {
                const char *c = wk_array_get(skipped_chars, i, const char *);
                wk_str_replace(b, c, "");
        }
        return b;
}
#line 1665 "xi.xi"
static void init_tie(WKTable *relations, WKBranch *x_bra) {
        WKStr *x_name = NULL;
#line 1708 "xi.xi"
        for (size_t i = 0; i < x_bra->lower->n; i++) {
                WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
                XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                if (y->type == XI_SNIPPET_NAME) {
                        XIToken *x_name_tok = wk_link_get(y->tokens->head, XIToken *);
                        x_name = x_name_tok->content;
                        break;
                }
        }
#line 1668 "xi.xi"
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
#line 1720 "xi.xi"
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
#line 1680 "xi.xi"
                if (x_tag_ref) {
                        WKStr *t = compact_text(x_tag_ref, xi_skipped_chars_in_name);
                        int id = -1;
#line 1741 "xi.xi"
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
#line 1684 "xi.xi"
#line 1770 "xi.xi"
                        if (has_appending_operator) wk_array_ins(tie->time_order, id + 1, x_bra, WKBranch *);
                        else if (has_prepending_operator) wk_array_ins(tie->time_order, id, x_bra, WKBranch *);
                        else {
                                fprintf(stderr, "Line %lu: The snippet needs an operator.\n",
                                                first_token_line_number(x_bra));
                                exit(EXIT_FAILURE);
                        }
#line 1686 "xi.xi"
                        wk_str_free(t);
                } else {
#line 1780 "xi.xi"
                        if (has_appending_operator) wk_array_add(tie->time_order, x_bra, WKBranch *);
                        else if (has_prepending_operator) wk_array_ins(tie->time_order, 0, x_bra, WKBranch *);
                        else {
                                fprintf(stderr, "Line %lu: The snippet needs an operator.\n",
                                                first_token_line_number(x_bra));
                                exit(EXIT_FAILURE);
                        }
#line 1690 "xi.xi"
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
#line 1794 "xi.xi"
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
#line 1814 "xi.xi"
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
#line 1925 "xi.xi"
#line 1890 "xi.xi"
static void xi_tie_box_free(WKBox *tie_box) {
        XITie *tie = wk_box_get(tie_box, XITie *);
        wk_array_free(tie->spatial_order);
        wk_array_free(tie->time_order);
        wk_array_free(tie->emissions);
        free(tie);
        wk_box_free(tie_box);
}
#line 1926 "xi.xi"
#line 2104 "xi.xi"
#line 1990 "xi.xi"
static WKStr *cascade_indents(WKList *indents) {
        WKStr *current_indent = wk_str(NULL);
        for (WKLink *it = indents->head; it; it = it->next) {
                WKStr *a = wk_link_get(it, WKStr *);
                wk_str_suffix(current_indent, a->body);
        }
        return current_indent;
}
#line 2105 "xi.xi"
#line 2062 "xi.xi"
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
#line 1933 "xi.xi"
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
#line 2082 "xi.xi"
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
#line 1977 "xi.xi"
                        } else {
#line 2003 "xi.xi"
                                WKStr *text = z_tok->content;
                                const char *head = text->body;
                                const char *tail = text->body + text->n;
                                if (show_line_number) {
#line 2034 "xi.xi"
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
#line 2008 "xi.xi"
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
#line 1979 "xi.xi"
			}
                }
                wk_str_free(current_indent);
        }
        wk_str_free(key);
}
#line 2106 "xi.xi"
#line 2920 "xi.xi"
#line 2289 "xi.xi"
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
#line 2233 "xi.xi"
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
#line 2261 "xi.xi"
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
#line 2306 "xi.xi"
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
#line 2281 "xi.xi"
                }
        }
}
#line 2330 "xi.xi"
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
#line 2921 "xi.xi"
#line 2489 "xi.xi"
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
#line 2370 "xi.xi"
static void output_snippet_with_name(WKTree *syntax_tree, WKBranch *x_bra, WKTable *relations, FILE *output) {
        XISyntax *x = wk_branch_get(x_bra, XISyntax *);
        WKStr *x_name = NULL;
        WKStr *x_id_text = wk_str(NULL);
        wk_str_printf(x_id_text, "%lu", x->id);
#line 2411 "xi.xi"
        if (xi_fmt->snippet_start->n > 0) {
                XIToken *x_lang_tok = find_language_token(x_bra);
                WKStr *a = wk_str(xi_fmt->snippet_start->body);
                if (x_lang_tok) {
                        wk_str_replace(a, "${language}", x_lang_tok->content->body);
                }
                fprintf(output, "%s", a->body);
                wk_str_free(a);
        }
#line 2376 "xi.xi"
        for (size_t i = 0; i < x_bra->lower->n; i++) {
                WKBranch *y_bra = wk_array_get(x_bra->lower, i, WKBranch *);
                XISyntax *y = wk_branch_get(y_bra, XISyntax *);
                XIToken *y_tok = y->tokens ? wk_link_get(y->tokens->head, XIToken *) : NULL;
                switch (y->type) {
                case XI_SNIPPET_NAME:
#line 2437 "xi.xi"
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
#line 2383 "xi.xi"
                        break;
                case XI_SNIPPET_TAG_REFERENCE:
#line 2557 "xi.xi"
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
#line 2386 "xi.xi"
                        break;
                case XI_SNIPPET_APPENDING_OPERATOR:
#line 2574 "xi.xi"
                        if (xi_fmt->snippet_appending_operator->n > 0) {
                                fprintf(output, "%s", xi_fmt->snippet_appending_operator->body);
                        } else fprintf(output, " %s", xi_symbols->snippet_appending_mark->body);
#line 2389 "xi.xi"
                        break;
                case XI_SNIPPET_PREPENDING_OPERATOR:
#line 2580 "xi.xi"
                        if (xi_fmt->snippet_prepending_operator->n > 0) {
                                fprintf(output, "%s", xi_fmt->snippet_prepending_operator->body);
                        } else fprintf(output, " %s", xi_symbols->snippet_prepending_mark->body);
#line 2392 "xi.xi"
                        break;
                case XI_SNIPPET_TAG:
#line 2588 "xi.xi"
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
#line 2395 "xi.xi"
                        break;
                case XI_SNIPPET_CONTENT:
#line 2608 "xi.xi"
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
#line 2631 "xi.xi"
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
#line 2620 "xi.xi"
                                        } else {
                                                fprintf(stderr, "Line %lu: the snippet <%s> not defined!\n",
                                                                z_tok->line_number, z_tok->content->body);
                                        }
                                }
                        }
#line 2398 "xi.xi"
                        break;
                default: ;
                }
        }
#line 2688 "xi.xi"
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
#line 2403 "xi.xi"
#line 2423 "xi.xi"
        if (xi_fmt->snippet_stop->n > 0) {
                XIToken *x_lang_tok = find_language_token(x_bra);
                WKStr *a = wk_str(xi_fmt->snippet_stop->body);
                if (x_lang_tok) {
                        wk_str_replace(a, "${language}", x_lang_tok->content->body);
                }
                fprintf(output, "%s", a->body);
                wk_str_free(a);
        }
#line 2404 "xi.xi"
        wk_str_free(x_id_text);
}
#line 2353 "xi.xi"
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
#line 2922 "xi.xi"
#line 2784 "xi.xi"
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
#line 2803 "xi.xi"
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
#line 2854 "xi.xi"
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
#line 2842 "xi.xi"
                        break;
                default: ;
                }
        }
#line 2893 "xi.xi"
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
#line 2847 "xi.xi"
        wk_str_free(x_id_text);
}
#line 2760 "xi.xi"
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
#line 2923 "xi.xi"
#line 3035 "xi.xi"
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
#line 3134 "xi.xi"
int main(int argc, char **argv) {
#line 2949 "xi.xi"
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
#line 3136 "xi.xi"
        if (help_opt) {
#line 3000 "xi.xi"
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
#line 3138 "xi.xi"
        } else {
#line 3067 "xi.xi"
#line 238 "xi.xi"
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
#line 3068 "xi.xi"
#line 274 "xi.xi"
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
#line 2162 "xi.xi"
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
#line 3069 "xi.xi"
#line 328 "xi.xi"
                FILE *xi_file = xi_file_path ? fopen(xi_file_path, "r") : stdin;
                if (!xi_file) {
                	fprintf(stderr, "Failed to open %s\n", xi_file_path);
                	exit(EXIT_FAILURE);
                }
#line 3070 "xi.xi"
                WKList *xi_tokens = xi_lexer(xi_file);
                WKTree *xi_tree = xi_parser(xi_tokens);
#line 1642 "xi.xi"
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
#line 3073 "xi.xi"
#line 1884 "xi.xi"
                wk_free_bus_connect("XITie *", xi_tie_box_free);
#line 3074 "xi.xi"
                WKTable *relations = xi_create_relations(xi_tree->root);
#line 3140 "xi.xi"
                if (tangle_opt) {
#line 3087 "xi.xi"
#line 3016 "xi.xi"
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
#line 3087 "xi.xi" /* 得到 entrances 与 outputs */
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
#line 3142 "xi.xi"
                }
                if (weave_opt) {
#line 3120 "xi.xi"
                        FILE *xi_output = fopen(output_opt, "w");
                        spread_language_mark(xi_tree, relations);
                        if (yaml_opt) {
                                xi_yaml_output(xi_tree, relations, xi_fmt, xi_output);
                        } else xi_weave(xi_tree, relations, xi_output);
                        fclose(xi_output);
#line 3145 "xi.xi"
                }
#line 308 "xi.xi"
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
#line 338 "xi.xi"
                fclose(xi_file);
#line 413 "xi.xi"
                for (size_t i = 0; i < xi_skipped_chars->n; i++) {
                        WKStr *s = wk_array_get(xi_skipped_chars, i, WKStr *);
                        wk_str_free(s);
                }
                wk_array_free(xi_skipped_chars);
#line 1657 "xi.xi"
                wk_array_free(xi_skipped_chars_in_name);
#line 2207 "xi.xi"
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
#line 3080 "xi.xi"
                wk_table_free(relations);
                xi_delete_syntax_tree(xi_tree);
#line 3147 "xi.xi"
        }
        return 0;
}