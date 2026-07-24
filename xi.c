#include <stdint.h> /* for SIZE_MAX */
/* 该头文件已包含 wk-str.h, wk-array.h, wk-list.h, wk-table.h 等 */
#include <wk-cfg.h>
#include <wk-tree.h>
#include <wk-cli.h>
WKArray *skipped_chars; /* 空白字符集 */
WKArray *skipped_chars_in_snippet_name;
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
typedef struct {
        XITokenType type;
        size_t line_number;
        WKStr *content;
} XIToken;
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
#define XI_USER_CONFIG(user_config, xi_symbols, item) do { \
        WKBox *v = wk_table_query(user_config, wk_box_ref(#item, const char *)); \
	if (v) { \
                if (xi_symbols->item) wk_str_free(xi_symbols->item); \
                xi_symbols->item = wk_str(wk_box_get(v, WKStr *)->body); \
        } \
} while (0)
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
static int read_char(FILE *src_file, WKStr *cache) {
        int c = fgetc(src_file);
        if (c != EOF) {
                wk_str_suffix_char(cache, (char)c);
                return c;
        } else return EOF;
}
static const char *xi_advance(const char *a,
                              const char *b,
                              const char *c,
                              int dir) {
	if (c <= a && dir < 0) return c;
	if (c >= b && dir >= 0) return c;
        while (1) {
		const char *u = c;
                for (size_t i = 0; i < skipped_chars->n; i++) {
                        WKStr *s = wk_array_get(skipped_chars, i, WKStr *);
                        const char *p = dir < 0 ? c - s->n : c;
                        if (p < a || p + s->n > b) continue;
			if (memcmp(s->body, p, s->n) == 0) {
				c = dir < 0 ? p : p + s->n;
			}
                }
                if (u == c) break;
        }
        return c;
}
static bool tail_is_snippet_delimiter(WKStr *cache,
                                      WKStr *snippet_delimiter) {
        if (snippet_delimiter->n > cache->n) return false;
        /* 令 p 指向缓冲区末尾 snippet_delimiter->n 个字节之首 */
        const char *p = cache->body + cache->n - snippet_delimiter->n;
        if (strcmp(p, snippet_delimiter->body) != 0) return false;
        if (p == cache->body) return true;
        /* 构造字符串区间 [a, b] */
        const char *a = cache->body;
        const char *b = p;
        /* 向左探测 */
        p = xi_advance(a, b, p, -1);
        if (p == a) return true;
        else return (*(--p) == '\n') ? true : false;
}
static void add_snippet(WKList *tokens, WKStr *cache) {
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_TEXT;
        size_t n;
        WKLink *tail = tokens->tail;
        if (tail) {
                XIToken *last_token = wk_link_get(tail, XIToken *);
                n = last_token->line_number;
                for (const char *p = last_token->content->body; *p != '\0'; p++) {
                        if (*p == '\n') n++;
                }
        } else  n = 1;
        snippet->line_number = n;
        snippet->content = cache;
        wk_list_suffix(tokens, snippet, XIToken *);
}
static void add_snippet_delimiter(WKList *tokens, WKStr *snippet_delimiter) {
        XIToken *snippet = malloc(sizeof(XIToken));
        snippet->type = XI_SNIPPET_DELIMITER;
        size_t n;
        WKLink *tail = tokens->tail;
        if (tail) {
                XIToken *last_token = wk_link_get(tail, XIToken *);
                n = last_token->line_number;
                for (const char *p = last_token->content->body; *p != '\0'; p++) {
                        if (*p == '\n') n++;
                }
        } else  n = 1;
        snippet->line_number = n;
        snippet->content = wk_str(snippet_delimiter->body);
        wk_list_suffix(tokens, snippet, XIToken *);
}
static bool here_is_me(const char *a,
                       const char *b,
                       const char *c,
                       int dir,
                       WKStr *me) {
        size_t n = me->n;
        const char *p = dir < 0 ? (c - n + 1) : c;
        if (p < a) return false;
        if (p + n > b) return false;
        return memcmp(me->body, p, n) == 0 ? true : false;
}
static const char *find_name_delimiter(XIToken *t,
                                       WKStr *delimiter,
                                       WKStr *continuation) {
        /* 构造字符串区间 [a, b) */
        const char *a = t->content->body;
        if (*a == '\n') return NULL;
        /* b 为 NULL 或指向片段名字界限符首字节 */
        const char *b = strstr(a, delimiter->body);
        if (!b || b == a) return NULL;
        /* 检测 [a, b) 是否含有合法的片段名字界限符 */
        const char *p = b;
        enum {IDLE, LINEBREAK, SUCCESS, FAILURE} state = IDLE;
        while (1) { /* 逆序遍历 [a, b) */
                p = xi_advance(a, b, p, -1);
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
			fprintf(stderr, "Illegal state in line %lu", t->line_number);
                }
                if (state == FAILURE || state == SUCCESS) break;
        }
        return (state == SUCCESS) ? b : NULL;
}
static WKStr *extract_block_at_head(XIToken *t,
					  WKStr *start_mark,
					  WKStr *stop_mark)
{
        WKStr *result = NULL;
        const char *block_start = NULL;
        const char *block_stop = NULL;
        size_t new_line_number = t->line_number;
        const char *a = t->content->body;
        const char *b = a + t->content->n;
        const char *p = a;
        size_t m = start_mark->n;
        size_t n = stop_mark->n;
        enum {IDLE, MAYBE_MARK, FAILURE, SUCCESS} state = IDLE;
        while (1) { /* 正序遍历 [a, b - 1] */
                p = xi_advance(a, b, p, 1);
                switch (state) {
                case IDLE:
                        if (*p == '\n') new_line_number++;
                        else if (here_is_me(a, b, p, 1, start_mark)) {
				block_start = p;
				p += (m - 1);
                                state = MAYBE_MARK;
                        } else state = FAILURE;
                        break;
                case MAYBE_MARK:
                        if (here_is_me(a, b, p, 1, stop_mark)) {
				p += (n - 1);
                                block_stop = p + 1;
                                state = SUCCESS;
                        } else state = MAYBE_MARK;
                        break;
                default:
                        fprintf(stderr, "Illegal state in line %lu.", t->line_number);
                }
                if (state == SUCCESS) { /* 从记号内容里消除块标记 */
                        result = wk_str(NULL);
                        const char *head = block_start + m;
                        const char *tail = block_stop - n;
                        for (const char *q = head; q != tail; q++) {
                                wk_str_suffix_char(result, *q);
                        }
                        WKStr *new_content = wk_str(NULL);
                        for (const char *q = block_stop; *q != '\0'; q++) {
                                wk_str_suffix_char(new_content, *q);
                        }
                        wk_str_free(t->content);
                        /* 更新记号内容 */
                        t->content = new_content;
                        t->line_number = new_line_number;
                        break;
                } else if (state == FAILURE || *p == '\0') break;
                else p++;
        }
        return result;
}
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
static WKStr *extract_operator(XIToken *t, WKStr *operator) {
        /* 构造字符串区间 [a, b) */
        const char *a = t->content->body;
        const char *b = strstr(a, operator->body);
        if (!b) return NULL;
        bool is_legal = true;
        if (b > a) {
                const char *p = b;
                p = xi_advance(a, b, p, -1);
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
static WKPair *find_snippet_reference(WKStr *content,
                                      WKStr *reference_start_mark,
                                      WKStr *reference_stop_mark,
                                      WKStr *continuation)
{
        const char *a = content->body, *begin = NULL, *end = NULL;
        enum {IDLE, MAYBE_LINEBREAK, FAILURE} state;
        while (1) {
                begin = strstr(a, reference_start_mark->body);
                if (!begin) return NULL;
                end = strstr(a, reference_stop_mark->body);
                if (!end || begin >= end) return NULL;
                /*区间 [c, p) 是片段名 */
                const char *p = end;
                const char *c = begin + reference_start_mark->n;
                /* 逆序遍历 [c, end)，校验片段名是否合法 */
                bool legal = true;
                state = IDLE;
                while (1) {
                        p = xi_advance(c, end, p, -1);
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
                                fprintf(stderr, "Illegal state in <<< %s >>>.", a);
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
static void split_snippet(WKList *tokens,
                          WKLink *x,
                          WKPair *snippet_reference,
                          WKStr *snippet_name_continuation,
                          WKStr *snippet_reference_start_mark,
                          WKStr *snippet_reference_stop_mark)
{
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
        WKStr *b = wk_str(snippet_reference_start_mark->body);
        XIToken *t_b = malloc(sizeof(XIToken));
        t_b->type = XI_SNIPPET_REFERENCE_START_MARK;
        t_b->line_number = line_number;
        t_b->content = b;
        /* 片段名称 */
        WKStr *c = wk_str(NULL);
        const char *left = ref_start + snippet_reference_start_mark->n;
        const char *right = ref_stop - snippet_reference_stop_mark->n;
        for (const char *p = left; p != right; p++) {
                wk_str_suffix_char(c, *p);
                if (*p == '\n') line_number++;
        }
        XIToken *t_c = malloc(sizeof(XIToken));
        t_c->type = XI_TEXT;
        t_c->line_number = t_b->line_number;
        t_c->content = c;
        /* 片段引用结束标记 */
        WKStr *d = wk_str(snippet_reference_stop_mark->body);
        XIToken *t_d = malloc(sizeof(XIToken));
        t_d->type = XI_SNIPPET_REFERENCE_STOP_MARK;
        t_d->line_number = line_number;
        t_d->content = d;
        /* 片段引用之后的内容 */
        WKStr *e = wk_str(NULL);
        bool after_reference = true;
        for (const char *p = ref_stop; *p != '\0'; p++) {
                if (after_reference) {
                         /* 忽略与片段引用之后与片段引用终止符同一行的文本 */
                        if (*p == '\n') {
                                after_reference = false;
                                wk_str_suffix_char(e, *p);
                        }
                } else wk_str_suffix_char(e, *p);
        }
        /* 更新记号列表 */
        WK_LIST_INSF(tokens, x, &t_a);
        WK_LIST_INSF(tokens, x, &t_b);
        WK_LIST_INSF(tokens, x, &t_c);
        WK_LIST_INSF(tokens, x, &t_d);
        wk_str_free(t->content);
        t->content = e;
        t->line_number = line_number;
}
static void delete_tokens(WKList *tokens) {
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                wk_str_free(t->content);
                free(t);
        }
        wk_list_free(tokens);
}
static WKList *xi_lexer(FILE *src_file, XISymbols *xi_symbols) {
        skipped_chars = wk_array(WKStr *);
        wk_array_add(skipped_chars, wk_str(" "), WKStr *);
        wk_array_add(skipped_chars, wk_str("\t"), WKStr *);
        wk_array_add(skipped_chars, wk_str("　"), WKStr *);
        WKList *xi_tokens = wk_list(XIToken *);
        WKStr *cache = wk_str(NULL);
        while (1) {
                int status = read_char(src_file, cache);
                if (status == EOF) break;
                else {
                        bool t = tail_is_snippet_delimiter(cache, xi_symbols->snippet_delimiter);
                        if (t) {
                                size_t length = xi_symbols->snippet_delimiter->n;
                                size_t begin = cache->n - length;
                                wk_str_del(cache, begin, length);
                                add_snippet(xi_tokens, cache);
                                add_snippet_delimiter(xi_tokens, xi_symbols->snippet_delimiter);
                                cache = wk_str(NULL); /* 刷新 cache */
                        }
                }
        }
        if (cache->n > 0) {
                add_snippet(xi_tokens, cache);
        } else wk_str_free(cache);
        WKLink *it = xi_tokens->head;
        while (it) {
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_TEXT) {
                        const char *p = find_name_delimiter(t,
                                                           xi_symbols->snippet_name_delimiter,
                                                           xi_symbols->snippet_name_continuation);
                        if (p) {
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
                                /* 删除 it */
                                WKLink *next = it->next;
        			wk_str_free(t->content);
                                free(t);
        			wk_list_del(xi_tokens, it);
                                /* 在 next 之前插入新节点 */
        			WK_LIST_INSF(xi_tokens, next, &snippet_name);
                                WK_LIST_INSF(xi_tokens, next, &snippet_name_delimiter);
                                WK_LIST_INSF(xi_tokens, next, &snippet);
                                /* 调整迭代指针 */
                                it = next->prev;
                        }
                }
                it = it->next;
        }
        it = xi_tokens->head;
        while (1) {
                if (!it) break;
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_TEXT) {
                        WKLink *prev = it->prev;
                        if (prev) {
                                XIToken *a = wk_link_get(prev, XIToken *);
                                if (a->type == XI_SNIPPET_NAME_DELIMITER) {
                                        WKStr *language = extract_block_at_head(t,
                                                                                xi_symbols->language_start_mark,
                                                                                xi_symbols->language_stop_mark);
                                        if (language) {
                                                add_block(xi_tokens,
                                                          it,
                                                          language,
                                                          xi_symbols->language_start_mark,
                                                          XI_LANGUAGE_START_MARK,
                                                          xi_symbols->language_stop_mark,
                                                          XI_LANGUAGE_STOP_MARK);
                                        }
                                }
                        }
                }
                it = it->next;
        }
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
                                        WKStr *tag_reference = extract_block_at_head(t,
                                                                                     xi_symbols->tag_start_mark,
                                                                                     xi_symbols->tag_stop_mark);
                                        if (tag_reference) {
                                                add_block(xi_tokens,
                                                          it,
                                                          tag_reference,
                                                          xi_symbols->tag_start_mark,
                                                          XI_TAG_START_MARK,
                                                          xi_symbols->tag_stop_mark,
                                                          XI_TAG_STOP_MARK);
                                        }
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
                                }
                        }
                }
                it = it->next;
        }
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
                                        WKStr *tag_mark = extract_block_at_head(t,
                                                                                xi_symbols->tag_start_mark,
                                                                                xi_symbols->tag_stop_mark);
                                        if (tag_mark) {
                                                add_block(xi_tokens,
                                                          it,
                                                          tag_mark,
                                                          xi_symbols->tag_start_mark,
                                                          XI_TAG_START_MARK,
                                                          xi_symbols->tag_stop_mark,
                                                          XI_TAG_STOP_MARK);
                                        }
                                }
                        }
                }
                it = it->next;
        }
        it = xi_tokens->head;
        while (1) {
                if (!it) break;
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_TEXT) {
                        WKLink *prev = it->prev;
                        if (prev) {
                                XIToken *t_prev = wk_link_get(prev, XIToken *);
                                if (t_prev->type != XI_SNIPPET_DELIMITER
                                    || t_prev->type != XI_LANGUAGE_START_MARK
                                    || t_prev->type != XI_TAG_START_MARK) {
                                        while (1) {
                                                WKPair *snippet_reference
                                                        = find_snippet_reference(t->content,
                                                                                 xi_symbols->snippet_reference_start_mark,
                                                                                 xi_symbols->snippet_reference_stop_mark,
                                                                                 xi_symbols->snippet_name_continuation);
                                                if (snippet_reference) {
                                                        split_snippet(xi_tokens,
                                                                      it,
                                                                      snippet_reference,
                                                                      xi_symbols->snippet_name_continuation,
                                                                      xi_symbols->snippet_reference_start_mark,
                                                                      xi_symbols->snippet_reference_stop_mark);
                                                        wk_box_free(snippet_reference->x);
                                                        wk_box_free(snippet_reference->y);
                                                        free(snippet_reference);
                                                } else break;
                                        }
                                }
                        }
                }
                it = it->next;
        }
        /* 为记号列表尾部追加一个片段界限符，以便于后续程序处理 */
        XIToken *last_token = wk_link_get(xi_tokens->tail, XIToken *);
        if (last_token->type != XI_SNIPPET_DELIMITER) {
                add_snippet_delimiter(xi_tokens, xi_symbols->snippet_delimiter);
        }
        return xi_tokens;
}
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
typedef struct {
        XISyntaxType type;
        WKList *tokens;
        size_t id;
} XISyntax;
static WKTree *syntax_tree_init(WKList *tokens) {
        WKTree *xi_tree = wk_tree(XISyntax *);
        wk_tree_add(xi_tree, NULL, NULL, XISyntax *); /* 根节点为空 */
        size_t id = 1; /* 有名片段的 id */
        for (WKLink *it = tokens->head; it; it = it->next) {
                XIToken *t = wk_link_get(it, XIToken *);
                if (t->type == XI_SNIPPET_DELIMITER) {
                        WKLink *prev = it->prev;
                        if (prev) {
                                XISyntax *snippet = malloc(sizeof(XISyntax));
                                /* 默认类型，后续会确定它是否为有名片段 */
                                snippet->type = XI_SNIPPET;
                                snippet->tokens = wk_list(XIToken *);
                                snippet->id = 0;
                                WKLink *a = prev;
                                while (1) {
                                        if (a) {
                                                XIToken *t_a = wk_link_get(a, XIToken *);
                                                if (t_a->type == XI_SNIPPET_DELIMITER) break;
                                                else {
                                                        if (t_a->type == XI_SNIPPET_NAME_DELIMITER) {
                                                                snippet->type = XI_SNIPPET_WITH_NAME;
                                                                snippet->id = id++;
                                                        }
                                                        wk_list_prefix(snippet->tokens, t_a, XIToken *);
                                                }
                                        } else break;
                                        a = a->prev;
                                }
                                /* 之前在记号列表尾部强行插入了一个片段界限记号，
                                   它可能会导致语法节点的内容为空，故而需要予以忽略 */
                                if (snippet->tokens) {
                                        wk_tree_add(xi_tree, xi_tree->root, snippet, XISyntax *);
                                } else free(snippet);
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
        /* 有用的记号已转移到语法树，销毁记号列表容器已完成使命 */
        wk_list_free(tokens);
        return xi_tree;
}
static void add_branches_for_snippet_with_name(WKTree *syntax_tree, WKBranch *x)
{
        XISyntax *snippet = wk_branch_get(x, XISyntax *);
        WKLink *it = snippet->tokens->head;
        XIToken *current_token = wk_link_get(it, XIToken *);
        XISyntax *snippet_name = malloc(sizeof(XISyntax));
        snippet_name->type = XI_SNIPPET_NAME;
        snippet_name->tokens = wk_list(XIToken *);
        snippet_name->id = 0;
        wk_list_suffix(snippet_name->tokens, current_token, XIToken *);
        wk_tree_add(syntax_tree, x, snippet_name, XISyntax *);
        it = it->next->next; /* 跳过片段名字界限符 */
        current_token = wk_link_get(it, XIToken *);
        if (current_token->type == XI_LANGUAGE_START_MARK) {
                XISyntax *language = malloc(sizeof(XISyntax));
                it = it->next;
                current_token = wk_link_get(it, XIToken *);
                language->type = XI_SNIPPET_LANGUAGE;
                language->tokens = wk_list(XIToken *);
                language->id = 0;
                wk_list_suffix(language->tokens, current_token, XIToken *);
                wk_tree_add(syntax_tree, x, language, XISyntax *);
                it = it->next;
                current_token = wk_link_get(it, XIToken *);
                it = it->next; /* 跳过语言标记终结符 */
        }
        current_token = wk_link_get(it, XIToken *);
        if (current_token->type == XI_TAG_START_MARK) {
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
                        XISyntax *tag_ref = malloc(sizeof(XISyntax));
                        it = it->next;
                        current_token = wk_link_get(it, XIToken *);
                        tag_ref->type = XI_SNIPPET_TAG_REFERENCE;
                        tag_ref->tokens = wk_list(XIToken *);
                        tag_ref->id = 0;
                        wk_list_suffix(tag_ref->tokens, current_token, XIToken *);
                        wk_tree_add(syntax_tree, x, tag_ref, XISyntax *);
                        /* 跳过标签引用结束符 */
                        it = it->next->next;
                }
        }
        current_token = wk_link_get(it, XIToken *);
        XISyntax *operator = NULL;
        if (current_token->type == XI_SNIPPET_APPENDING_MARK) {
                operator = malloc(sizeof(XISyntax));
                operator->type = XI_SNIPPET_APPENDING_OPERATOR;
                operator->id = 0;
        }
        if (current_token->type == XI_SNIPPET_PREPENDING_MARK) {
                operator = malloc(sizeof(XISyntax));
                operator->type = XI_SNIPPET_PREPENDING_OPERATOR;
                operator->id = 0;
        }
        if (operator) {
                operator->tokens = wk_list(XIToken *);
                wk_list_suffix(operator->tokens, current_token, XIToken *);
                wk_tree_add(syntax_tree, x, operator, XISyntax *);
                it = it->next;
        }
        current_token = wk_link_get(it, XIToken *);
        if (current_token->type == XI_TAG_START_MARK) {
                XISyntax *tag = malloc(sizeof(XISyntax));
                it = it->next;
                current_token = wk_link_get(it, XIToken *);
                tag->type = XI_SNIPPET_TAG;
                tag->tokens = wk_list(XIToken *);
                tag->id = 0;
                wk_list_suffix(tag->tokens, current_token, XIToken *);
                wk_tree_add(syntax_tree, x, tag, XISyntax *);
                /* 跳过标签结束符 */
                it = it->next->next;
        }
        XISyntax *content = malloc(sizeof(XISyntax));
        content->type = XI_SNIPPET_CONTENT;
        content->tokens = wk_list(XIToken *);
        content->id = 0;
        for (WKLink *it_a = it; it_a; it_a = it_a->next) {
                current_token = wk_link_get(it_a, XIToken *);
                wk_list_suffix(content->tokens, current_token, XIToken *);
        }
        wk_tree_add(syntax_tree, x, content, XISyntax *);
        for (it = snippet->tokens->head; it; it = it->next) {
                current_token = wk_link_get(it, XIToken *);
                if (current_token->type == XI_SNIPPET_NAME_DELIMITER
                    || current_token->type == XI_LANGUAGE_START_MARK
                    || current_token->type == XI_LANGUAGE_STOP_MARK
                    || current_token->type == XI_TAG_START_MARK
                    || current_token->type == XI_TAG_STOP_MARK) {
                        wk_str_free(current_token->content);
                        free(current_token);
                }
        }
        wk_list_free(snippet->tokens);
        snippet->tokens = NULL;
}
static void add_branches_for_snippet_content(WKTree *syntax_tree, WKBranch *x)
{
        XISyntax *content = wk_branch_get(x, XISyntax *);
        WKLink *it = content->tokens->head;
        while (it) {
                XIToken *token = wk_link_get(it, XIToken *);
                if (token->type == XI_SNIPPET_REFERENCE_START_MARK) {
                        it = it->next;
                        token = wk_link_get(it, XIToken *);
                        /* 构造片段引用 */
                        XISyntax *reference = malloc(sizeof(XISyntax));
                        reference->type = XI_SNIPPET_REFERENCE;
                        reference->tokens = wk_list(XIToken *);
                        reference->id = 0;
                        wk_list_suffix(reference->tokens, token, XIToken *);
                        wk_tree_add(syntax_tree, x, reference, XISyntax *);
                        /* 让片段引用结束符作为当前节点 */
                        it = it->next;
                } else {
                        XISyntax *text = malloc(sizeof(XISyntax));
                        text->type = XI_SNIPPET_TEXT;
                        text->tokens = wk_list(XIToken *);
                        text->id = 0;
                        wk_list_suffix(text->tokens, token, XIToken *);
                        wk_tree_add(syntax_tree, x, text, XISyntax *);
                }
                it = it->next;
        }
        /* 释放 content->tokens */
        for (it = content->tokens->head; it; it = it->next) {
                XIToken *token = wk_link_get(it, XIToken *);
                if (token->type == XI_SNIPPET_REFERENCE_START_MARK
                    || token->type == XI_SNIPPET_REFERENCE_STOP_MARK) {
                        wk_str_free(token->content);
                        free(token);
                }
        }
        wk_list_free(content->tokens);
        content->tokens = NULL;
}
static void delete_syntax_body(WKBranch *root) {
        if (!root) return;
        XISyntax *body = wk_branch_get(root, XISyntax *);
        if (body) {
                if (body->tokens) delete_tokens(body->tokens);
                free(body);
        }
        if (root->lower) {
                for (size_t i = 0; i < root->lower->n; i++) {
                        delete_syntax_body(wk_array_get(root->lower, i, WKBranch *));
                }
        }
}
static void delete_syntax_tree(WKTree *syntax_tree) {
        delete_syntax_body(syntax_tree->root);
        wk_tree_free(syntax_tree);
}
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

static WKTree *xi_parser(WKList *tokens) {
        WKTree *xi_tree = syntax_tree_init(tokens);
        WKArray *branches = xi_tree->root->lower;
        for (size_t i = 0; i < branches->n; i++) {
                WKBranch *p = wk_array_get(branches, i, WKBranch *);
                XISyntax *snippet = wk_branch_get(p, XISyntax *);
                if (snippet->type == XI_SNIPPET_WITH_NAME) {
                        add_branches_for_snippet_with_name(xi_tree, p);
                }
        }
        WKArray *second_level_branches = xi_tree->root->lower;
        for (size_t i = 0; i < second_level_branches->n; i++) {
                WKBranch *p = wk_array_get(second_level_branches, i, WKBranch *);
                XISyntax *a = wk_branch_get(p, XISyntax *);
                if (a->type == XI_SNIPPET_WITH_NAME) {
                        WKArray *third_level_branches = p->lower;
                        for (size_t j = 0; j < third_level_branches->n; j++) {
                                WKBranch *q = wk_array_get(third_level_branches, j, WKBranch *);
                                XISyntax *b = wk_branch_get(q, XISyntax *);
                                if (b->type == XI_SNIPPET_CONTENT) {
                                        add_branches_for_snippet_content(xi_tree, q);
                                }
                        }
                }
        }
        return xi_tree;
}
typedef struct {
        WKArray *time_order;
        WKArray *spatial_order;
        WKArray *emissions;
} XITie;
static WKStr *compact_text(WKStr *a) {
        WKStr *b = wk_str(a->body);
        for (size_t i = 0; i < skipped_chars_in_snippet_name->n; i++) {
                const char *c = wk_array_get(skipped_chars_in_snippet_name, i, const char *);
                wk_str_replace(b, c, "");
        }
        /* 去除引号 */
        wk_str_replace(b, "\'", "");
        wk_str_replace(b, "\"", "");
        /* 如有需要，可继续添加 */
        /* ... ... ... */
        return b;
}
static void init_tie(WKTable *relations, WKBranch *x) {
        WKStr *name = NULL;
        for (size_t i = 0; i < x->lower->n; i++) {
                WKBranch *p = wk_array_get(x->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(p, XISyntax *);
                if (a->type == XI_SNIPPET_NAME) {
                        XIToken *t = wk_link_get(a->tokens->head, XIToken *);
                        name = t->content;
                        break;
                }
        }
        if (!name) {
                fprintf(stderr, "Line %lu: Illegal snipet.\n", first_token_line_number(x));
                exit(EXIT_FAILURE);
        }
        WKStr *key = compact_text(name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);        
                bool has_appending_operator = false;
                bool has_prepending_operator = false;
                WKStr *tag_reference = NULL;
                for (size_t i = 0; i < x->lower->n; i++) {
                        WKBranch *p = wk_array_get(x->lower, i, WKBranch *);
                        XISyntax *a = wk_branch_get(p, XISyntax *);
                        if (a->type == XI_SNIPPET_TAG_REFERENCE) {
                                XIToken *t_a = wk_link_get(a->tokens->head, XIToken *);
                                tag_reference = t_a->content;
                        }
                        if (a->type == XI_SNIPPET_APPENDING_OPERATOR) {
                                has_appending_operator = true;
                                break;
                        }
                        if (a->type == XI_SNIPPET_PREPENDING_OPERATOR) {
                                has_prepending_operator = true;
                                break;
                        }
                }
                if (tag_reference) {
                        WKStr *t = compact_text(tag_reference);
                        int id = -1;
                        size_t hits = 0;
                        for (int i = 0; i < tie->time_order->n; i++) {
                                WKBranch *y = wk_array_get(tie->time_order, i, WKBranch *);
                                for (size_t j = 0; j < y->lower->n; j++) {
                                        WKBranch *p = wk_array_get(y->lower, j, WKBranch *);
                                        XISyntax *a = wk_branch_get(p, XISyntax *);
                                        if (a->type == XI_SNIPPET_TAG) {
                                                XIToken *tag_token = wk_link_get(a->tokens->head, XIToken *);
                                                WKStr *s = compact_text(tag_token->content);
                                                if (strcmp(s->body, t->body) == 0) {
                                                        id = i;
                                                        hits++;
                                                }
                                                wk_str_free(s);
                                        }
                                }
                        }
                        if (hits > 1) {
                                fprintf(stderr, "Line %lu: repeated tag <%s>!\n", first_token_line_number(x), t->body);
                                exit(EXIT_FAILURE);
                        }
                        if (id < 0) {
                                fprintf(stderr, "Line %lu: The referenced tag does not exist.", first_token_line_number(x));
                                exit(EXIT_FAILURE);
                        }
                        if (has_appending_operator) {
                                wk_array_ins(tie->time_order, id + 1, x, WKBranch *);
                        } else if (has_prepending_operator) {
                                wk_array_ins(tie->time_order, id, x, WKBranch *);
                        } else {
                                fprintf(stderr, "Line %lu: The snippet needs an operator.",
                                                first_token_line_number(x));
                                exit(EXIT_FAILURE);
                        }
                        wk_str_free(t);
                } else {
                        if (has_appending_operator) {
                                wk_array_add(tie->time_order, x, WKBranch *);
                        } else if (has_prepending_operator) {
                                wk_array_ins(tie->time_order, 0, x, WKBranch *);
                        } else {
                                fprintf(stderr, "Line %lu: The snippet needs an operator.",
                                                first_token_line_number(x));
                                exit(EXIT_FAILURE);
                        }
                }
                wk_array_add(tie->spatial_order, x, WKBranch *);
                wk_str_free(key);
        } else {
                XITie *tie = malloc(sizeof(XITie));
                tie->time_order = wk_array(WKBranch *);
                tie->spatial_order = wk_array(WKBranch *);
                tie->emissions = wk_array(WKBranch *);
                wk_array_add(tie->time_order, x, WKBranch *);
                wk_array_add(tie->spatial_order, x, WKBranch *);
                wk_table_add(relations, wk_pair(wk_box(key, WKStr *),
                                                wk_box(tie, XITie *)));
        }
}
static void create_relation(WKTable *relations,
                            WKBranch *x,
                            XISyntax *a,
                            WKArray *skipped_chars_in_snippet_name)
{
        XIToken *t = wk_link_get(a->tokens->head, XIToken *);
        WKStr *s = compact_text(t->content);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(s, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);
                wk_array_add(tie->emissions, x, WKBranch *);
        } else {
                fprintf(stderr, "Line %lu: The snippet named <%s> not existed.",
                                t->line_number, t->content->body);
                exit(EXIT_FAILURE);
        }
        wk_str_free(s);
}
/* root 为语法树根节点 */
static WKTable *xi_create_relations(WKBranch *root) {
        WKTable *relations = wk_table(WKStr *, XITie *);
        for (size_t i = 0; i < root->lower->n; i++) {
                WKBranch *it = wk_array_get(root->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(it, XISyntax *);
                if (a->type != XI_SNIPPET_WITH_NAME) continue;
                init_tie(relations, it);
        }
        for (size_t i = 0; i < root->lower->n; i++) {
                WKBranch *it = wk_array_get(root->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(it, XISyntax *);
                if (a->type != XI_SNIPPET_WITH_NAME) continue;
                WKBranch *content = wk_array_get(it->lower, it->lower->n - 1, WKBranch *);
                for (size_t j = 0; j < content->lower->n; j++) {
                        WKBranch *o_it = wk_array_get(content->lower, j, WKBranch *);
                        XISyntax *b = wk_branch_get(o_it, XISyntax *);
                        if (b->type != XI_SNIPPET_REFERENCE) continue;
                        create_relation(relations, it, b, skipped_chars_in_snippet_name);
                }
        }
        return relations;
}
static void xi_tie_box_free(WKBox *tie_box) {
        XITie *tie = wk_box_get(tie_box, XITie *);
        wk_array_free(tie->spatial_order);
        wk_array_free(tie->time_order);
        wk_array_free(tie->emissions);
        free(tie);
        wk_box_free(tie_box);
}
static WKStr *cascade_indents(WKList *indents) {
        WKStr *current_indent = wk_str(NULL);
        for (WKLink *it = indents->head; it; it = it->next) {
                WKStr *a = wk_link_get(it, WKStr *);
                wk_str_suffix(current_indent, a->body);
        }
        return current_indent;
}
static WKStr *last_blank_line(XISyntax *a) {
        WKStr *blank_line = wk_str(NULL);
        XIToken *t = wk_link_get(a->tokens->head, XIToken *);
        if (t->content->n > 1) {
                WKStr *u = t->content;
                const char *head = u->body;
                const char *tail = u->body + u->n;
                const char *p = tail;
                p = xi_advance(head, tail, p, -1);
                for (const char *q = p; *q != '\0'; q++) {
                        wk_str_suffix_char(blank_line, *q);
                }
        }
        return blank_line;
}
static void xi_tangle(const char *src_file_path,
                      WKTable *relations,
                      WKStr *entrance,
                      size_t begin, /* 区间开始 id */
                      size_t end,   /* 区间终止 id */
                      WKList *indents, /* 用于记录所引用的片段的缩进层次 */
                      bool show_line_number, /* 用于控制输出的内容是否包含行号 */
                      FILE *output) {
        WKStr *key = compact_text(entrance);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (!tie_box) {
                fprintf(stderr, "Snippet <%s> never existed!", entrance->body);
		exit(EXIT_FAILURE);
        }
	XITie *tie = wk_box_get(tie_box, XITie *);
        for (size_t i = 0; i < tie->time_order->n; i++) {
                WKBranch *snippet = wk_array_get(tie->time_order, i, WKBranch *);
                XISyntax *snippet_syntax = wk_branch_get(snippet, XISyntax *);
                if (snippet_syntax->id < begin || snippet_syntax->id > end) continue;
                WKBranch *body = wk_array_get(snippet->lower, snippet->lower->n - 1, WKBranch *);
		for (size_t j = 0; j < body->lower->n; j++) {
			WKBranch *it = wk_array_get(body->lower, j, WKBranch *);
                        XISyntax *a = wk_branch_get(it, XISyntax *);
                        XIToken *ta = wk_link_get(a->tokens->head, XIToken *);
                        WKStr *text = ta->content;
                        if (a->type == XI_SNIPPET_REFERENCE) {
                                if (j > 0) {
                                	WKBranch *prev = wk_array_get(body->lower, j - 1, WKBranch *);
                                        XISyntax *x = wk_branch_get(prev, XISyntax *);
                                        if (x->type == XI_SNIPPET_TEXT) {
                                                WKStr *blank_line = last_blank_line(x);
                                                wk_list_prefix(indents, blank_line, WKStr *);
                                        }
                                        XIToken *tx = wk_link_get(a->tokens->head, XIToken *);
                                        xi_tangle(src_file_path, relations, tx->content, begin, end, indents, show_line_number, output);
                                        /* 删除当前层次的缩进 */
                                	WKStr *current_indent = wk_link_get(indents->head, WKStr *);
                                        wk_str_free(current_indent);
                                	wk_list_del(indents, indents->head);
                                }
                        } else {
                                WKStr *cache = wk_str(NULL);
                                WKStr *current_indent = cascade_indents(indents); /* 将各层次的缩进串联起来 */
                                if (show_line_number) {
                                        size_t line = first_token_line_number(it);
                                        const char *p = text->body;
                                        p = xi_advance(text->body, text->body + text->n, p, 1);
                                        if (*p == '\n') {
                                                line++;
                                        }
                                        WKStr *line_number = wk_str(NULL);
                                        wk_str_printf(line_number, "#line %lu \"%s\"\n", line, src_file_path);
                                        wk_str_suffix(cache, line_number->body);
                                        wk_str_free(line_number);
                                
                                }
                                const char *head = text->body;
                                const char *tail = text->body + text->n;
                                /* 掐头 */
                                head = xi_advance(head, tail, head, 1);
                                if (*head == '\n') head++;
                                /* 去尾 */
                                tail = xi_advance(head, tail, tail, -1);
                                if (*(tail - 1) == '\n') tail--;
                                /* 有时会出现片段内容为空行的情况，此时 head > tail，需要排除这种情况 */
                                if (head < tail) {
                                        /* 由于掐头的原因，首行要添加缩进 */
                                        wk_str_suffix(cache, current_indent->body);
                                        /* 其余各行缩进 */
                                        for (const char *p = head; p != tail; p++) {
                                                wk_str_suffix_char(cache, *p);
                                                if (*p == '\n') {
                                                        wk_str_suffix(cache, current_indent->body);
                                                }
                                        }
                                        /* 去尾是为了避开最后一行增加额外的缩进，现在将其归还 */
                                        if (*tail == '\n') wk_str_suffix(cache, "\n");
                                }
                                fputs(cache->body, output);
                                wk_str_free(current_indent);
                                wk_str_free(cache);
			}
                }
        }
        wk_str_free(key);
}
static WKPair *snippet_area(WKTree *syntax_tree, const char *a, const char *b) {
        /* 为了稳健，需对 a，b 予以压缩 */
        WKStr *sa = wk_str(a);
        WKStr *sb = wk_str(b);
        WKStr *l = compact_text(sa);
        WKStr *r = compact_text(sb);
        /* 顺序遍历语法树第 2 层类型为 XI_SNIPPET 的节点，
           查找含字符串 a 者并获取其 id */
        WKArray *level2 = syntax_tree->root->lower;
        size_t u = 0;
        size_t v = SIZE_MAX;
        for (size_t i = 0; i < level2->n; i++) {
                WKBranch *it = wk_array_get(level2, i, WKBranch *);
                XISyntax *snippet = wk_branch_get(it, XISyntax *);
                if (snippet->type == XI_SNIPPET) {
                        WKStr *content = wk_link_get(snippet->tokens->head, XIToken *)->content;
                        /* 对 content 压缩 */
                        WKStr *target = compact_text(content);
                        if (strstr(target->body, l->body)) {
                                for (size_t j = i; j < level2->n; j++) {
                                        WKBranch *it_j = wk_array_get(level2, j, WKBranch *);
                                        XISyntax *snippet_j = wk_branch_get(it_j, XISyntax *);
                                        if (snippet_j->type == XI_SNIPPET_WITH_NAME) {
                                                u = snippet_j->id;
                                                wk_str_free(target);
                                                goto AREA_LEFT_DETERMINED;
                                        }
                                }
                        }
                        wk_str_free(target);
                }
        }
AREA_LEFT_DETERMINED:
        for (size_t i = level2->n; i > 0; i--) { /* 针对无符号下标的循环 */
                WKBranch *it = wk_array_get(level2, i - 1, WKBranch *);
                XISyntax *snippet = wk_branch_get(it, XISyntax *);
                if (snippet->type == XI_SNIPPET) {
                        WKStr *content = wk_link_get(snippet->tokens->head, XIToken *)->content;
                        /* 对 content 压缩 */
                        WKStr *target = compact_text(content);
                        if (strstr(target->body, r->body)) {
                                for (size_t j = i; j > 0; j--) {
                                        WKBranch *it_j = wk_array_get(level2, j - 1, WKBranch *);
                                        XISyntax *snippet_j = wk_branch_get(it_j, XISyntax *);
                                        if (snippet_j->type == XI_SNIPPET_WITH_NAME) {
                                                v = snippet_j->id;
                                                wk_str_free(target);
                                                goto AREA_RIGHT_DETERMINED;
                                        }
                                }
                        }
                        wk_str_free(target);
                }
        }
AREA_RIGHT_DETERMINED:
        wk_str_free(sa);
        wk_str_free(sb);
        wk_str_free(l);
        wk_str_free(r);
        return wk_pair(wk_box(u, size_t), wk_box(v, size_t));
}
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
static void get_thread(WKTable *relations, WKStr *entrance, WKList *thread) {
        WKStr *key = compact_text(entrance);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
        if (!tie_box) {
                fprintf(stderr, "Snippet <%s> never existed!", entrance->body);
		exit(EXIT_FAILURE);
        }
        XITie *tie = wk_box_get(tie_box, XITie *);
        for (size_t i = 0; i < tie->spatial_order->n; i++) {
                WKBranch *t = wk_array_get(tie->spatial_order, i, WKBranch *);
                WKBranch *x = wk_array_get(t->lower, t->lower->n - 1, WKBranch *);
                wk_list_prefix(thread, t, WKBranch *);
                for (size_t j = 0; j < x->lower->n; j++) {
                        WKBranch *it = wk_array_get(x->lower, j, WKBranch *);
                        XISyntax *a = wk_branch_get(it, XISyntax *);
                        if (a->type == XI_SNIPPET_REFERENCE) {
                                XIToken *ta = wk_link_get(a->tokens->head, XIToken *);
                                get_thread(relations, ta->content, thread);
                        }
                }
        }
        wk_str_free(key);
}
static void spread_language_mark_in_thread(WKTree *xi_tree, WKList *thread) {
        XIToken *language_token = NULL;
        for (WKLink *it = thread->head; it; it = it->next) {
                WKBranch *x = wk_link_get(it, WKBranch *);
                language_token = find_language_token(x);
                if (language_token) break;
        }
        if (!language_token) return;
        for (WKLink *it = thread->head; it; it = it->next) {
                WKBranch *x = wk_link_get(it, WKBranch *);
                /* 检测 x 是否含有语言标记 */
                XIToken *language_token_a = find_language_token(x);
                if (language_token_a) {
                        if (strcmp(language_token->content->body,
                                   language_token_a->content->body) != 0) {
                                fprintf(stderr, "Line %lu and %lu: "
                                                "there are two different language marks.",
                                                language_token->line_number,
                                                language_token_a->line_number);
                        }
                } else {
                        XIToken *new_language_token = malloc(sizeof(XIToken));
                        new_language_token->type = XI_TEXT;
                        new_language_token->content = wk_str(language_token->content->body);
                        /* 确定语言标记的行号 */
                        XISyntax *name = wk_branch_get(wk_array_get(x->lower, 0, WKBranch *), XISyntax *);
                        XIToken *name_token = wk_link_get(name->tokens->head, XIToken *);
                        size_t line_number = name_token->line_number;
                        for (char *p = name_token->content->body; *p != '\0'; p++) {
                                if (*p == '\n') line_number++;
                        }
                        new_language_token->line_number = line_number;
                        /* 将语言标记添加到语法树 */
                        XISyntax *new_language = malloc(sizeof(XISyntax));
                        new_language->type = XI_SNIPPET_LANGUAGE;
                        new_language->tokens = wk_list(XIToken *);
                        new_language->id = 0;
                        wk_list_suffix(new_language->tokens, new_language_token, XIToken *);
                        /* 新节点会作为 x 的第 2 个节点 */
                        wk_tree_ins(xi_tree, x, 1, new_language, XISyntax *);
                }
        }
}
static void spread_language_mark(WKTree *xi_tree, WKTable *relations) {
        for (size_t i = 0; i < xi_tree->root->lower->n; i++) {
                WKBranch *it = wk_array_get(xi_tree->root->lower, i, WKBranch *);
                XISyntax *a = wk_branch_get(it, XISyntax *);
                if (a->type == XI_SNIPPET_WITH_NAME) {
                        XISyntax *b = wk_branch_get(wk_array_get(it->lower,
                                                                 0,
                                                                 WKBranch *),
                                                    XISyntax *);
                        XIToken *tb = wk_link_get(b->tokens->head, XIToken *);
                        WKList *thread = wk_list(WKBranch *);
                        get_thread(relations, tb->content, thread);
                        spread_language_mark_in_thread(xi_tree, thread);
                        wk_list_free(thread);
                }
        }
}
WKArray *snippet_name_split(WKStr *name, WKStr *continuation) {
        WKArray *segments = wk_array(WKStr *);
        WKStr *a = wk_str(NULL);
        WKStr *b = NULL;
        const char *p = name->body;
        const char *q = name->body + name->n;
        WKStr *quad = wk_str("　");
        enum {INIT, WHITE_AREA} state = INIT;
        while (1) {
                if (*p == '\0') break;
                switch (state) {
                case INIT:
                        if (strstr(p, continuation->body) == p) {
                                p += continuation->n - 1;
                                wk_array_add(segments, a, WKStr *);
                                a = wk_str(NULL);
                                b = wk_str(NULL);
                                wk_str_suffix(b, continuation->body);
                                state = WHITE_AREA;
                        } else {
                                wk_str_suffix_char(a, *p);
                        }
                        break;
                case WHITE_AREA:
                        if (*p == '\n' || *p == ' ' || *p == '\t') {
                                wk_str_suffix_char(b, *p);
                        } else if (here_is_me(p, q, p, 1, quad)) {
                                wk_str_suffix(b, quad->body);
                        } else {
                                wk_array_add(segments, b, WKStr *);
                                wk_str_suffix_char(a, *p);
                                state = INIT;
                        }
                        break;
                default:
                        fprintf(stderr, "Illegal state in <%s>!\n", name->body);
                        exit(EXIT_FAILURE);
                }
                p++;
        }
        if (a->n > 0) wk_array_add(segments, a, WKStr *);
        else wk_str_free(a);
        wk_str_free(quad);
        return segments;
}
static void output_snippet_with_name(WKBranch *x,
                                     WKTable *relations,
                                     XISymbols *symbols,
                                     XIFmt *fmt,
                                     FILE *output) {
        WKStr *x_name = NULL;
        WKStr *id_text = wk_str(NULL);
        do {
                if (fmt->snippet_start->n > 0) {
                        XIToken *language = find_language_token(x);
                        WKStr *a = wk_str(fmt->snippet_start->body);
                        if (language) {
                                wk_str_replace(a, "${language}", language->content->body);
                        }
                        fprintf(output, "%s", a->body);
                        wk_str_free(a);
                }
                
        } while (0);
        for (size_t i = 0; i < x->lower->n; i++) {
                WKBranch *y = wk_array_get(x->lower, i, WKBranch *);
                XISyntax *y_syntax = wk_branch_get(y, XISyntax *);
                switch (y_syntax->type) {
                case XI_SNIPPET_NAME:
                        do {
                                XISyntax *y_syntax = wk_branch_get(y, XISyntax *);
                                XIToken *y_token = wk_link_get(y_syntax->tokens->head, XIToken *);
                                if (fmt->snippet_name->n > 0) {
                                        XISyntax *x_syntax = wk_branch_get(x, XISyntax *);
                                        wk_str_printf(id_text, "%lu", x_syntax->id);
                                        WKArray *segments = snippet_name_split(y_token->content,
                                                                               symbols->snippet_name_continuation);
                                        /* 输出片段定界符 */
                                        if (fmt->snippet_name_start->n > 0) {
                                                fprintf(output, "%s", fmt->snippet_name_start->body);
                                        } else {
                                                fprintf(output, "%s", symbols->snippet_delimiter->body);
                                        }
                                        /* 输出名字 */
                                        for (size_t j = 0; j < segments->n; j++) {
                                                WKStr *segment = wk_array_get(segments, j, WKStr *);
                                                if (j % 2 == 0) {
                                                        WKStr *snippet_name_t = wk_str(fmt->snippet_name->body);
                                                        wk_str_replace(snippet_name_t, "${name}", segment->body);
                                                        fprintf(output, "%s", snippet_name_t->body);
                                                        wk_str_free(snippet_name_t);
                                                } else { /* 输出续行符以及下一行的前导空白字符 */
                                                        if (fmt->snippet_name_continuation->n > 0) {
                                                                wk_str_replace(segment,
                                                                               symbols->snippet_name_continuation->body,
                                                                               fmt->snippet_name_continuation->body);
                                                                fprintf(output, "%s", segment->body);
                                                        } else {
                                                                fprintf(output, "%s", segment->body);
                                                        }
                                                }
                                                wk_str_free(segment);
                                        }
                                        /* 输出名字定界符 */
                                        if (fmt->snippet_name_stop->n > 0) {
                                                fprintf(output, "%s", fmt->snippet_name_stop->body);
                                        } else {
                                                fprintf(output, "%s", symbols->snippet_name_delimiter->body);
                                        }
                                        if (fmt->snippet_id->n > 0) {
                                                WKStr *snippet_id = wk_str(fmt->snippet_id->body);
                                                wk_str_replace(snippet_id, "${id}", id_text->body);
                                                fprintf(output, "%s", snippet_id->body);
                                                wk_str_free(snippet_id);
                                        }
                                        wk_array_free(segments);
                                } else fprintf(output, "%s%s%s",
                                                       symbols->snippet_delimiter->body,
                                                       y_token->content->body,
                                                       symbols->snippet_name_delimiter->body);
                                /* 保存片段名，留待输出片段引用者时用 */
                                x_name = y_token->content;
                        } while (0);
                        break;
                case XI_SNIPPET_TAG_REFERENCE:
                        do {
                                XISyntax *tag_ref = wk_branch_get(y, XISyntax *);
                                XIToken *tag_ref_token = wk_link_get(tag_ref->tokens->head, XIToken *);
                                if (fmt->snippet_tag_reference->n > 0) {
                                        XISyntax *x_syntax = wk_branch_get(x, XISyntax *);
                                        wk_str_printf(id_text, "%lu", x_syntax->id);
                                        WKStr *a = wk_str(fmt->snippet_tag_reference->body);
                                        wk_str_replace(a, "${id}", id_text->body);
                                        wk_str_replace(a, "${name}", tag_ref_token->content->body);
                                        fprintf(output, "%s", a->body);
                                        wk_str_free(a);
                                } else {
                                        fprintf(output, " %s%s%s",
                                                        symbols->tag_start_mark->body,
                                                        tag_ref_token->content->body,
                                                        symbols->tag_stop_mark->body);
                                }
                        } while (0);
                        break;
                case XI_SNIPPET_APPENDING_OPERATOR:
                        do {
                                if (fmt->snippet_appending_operator->n > 0) {
                                        fprintf(output, " %s", fmt->snippet_appending_operator->body);
                                } else fprintf(output, " %s", symbols->snippet_appending_mark->body);
                        } while (0);
                        break;
                case XI_SNIPPET_PREPENDING_OPERATOR:
                        do {
                                if (fmt->snippet_prepending_operator->n > 0) {
                                        fprintf(output, " %s", fmt->snippet_prepending_operator->body);
                                } else fprintf(output, " %s", symbols->snippet_prepending_mark->body);
                        } while (0);
                        break;
                case XI_SNIPPET_TAG:
                        do {
                                XISyntax *tag = wk_branch_get(y, XISyntax *);
                                XIToken *tag_token = wk_link_get(tag->tokens->head, XIToken *);
                                if (fmt->snippet_tag->n > 0) {
                                        XISyntax *x_syntax = wk_branch_get(x, XISyntax *);
                                        wk_str_printf(id_text, "%lu", x_syntax->id);
                                        WKStr *a = wk_str(fmt->snippet_tag->body);
                                        wk_str_replace(a, "${id}", id_text->body);
                                        wk_str_replace(a, "${name}", tag_token->content->body);
                                        fprintf(output, "%s", a->body);
                                        wk_str_free(a);
                                } else {
                                        fprintf(output, " %s%s%s",
                                                        symbols->tag_start_mark->body,
                                                        tag_token->content->body,
                                                        symbols->tag_stop_mark->body);
                                }
                        } while (0);
                        break;
                case XI_SNIPPET_CONTENT:
                        for (size_t j = 0; j < y->lower->n; j++) {
                                WKBranch *z = wk_array_get(y->lower, j, WKBranch *);
                                XISyntax *z_syntax = wk_branch_get(z, XISyntax *);
                                XIToken *z_token = wk_link_get(z_syntax->tokens->head, XIToken *);
                                if (z_syntax->type == XI_SNIPPET_TEXT) {
                                        fprintf(output, "%s", z_token->content->body);
                                } else {
                                        WKStr *key = compact_text(z_token->content);
                                        WKBox *tie_box = wk_table_query(relations, wk_box_ref(key, WKStr *));
                                        wk_str_free(key);
                                        if (!tie_box) {
                                                fprintf(stderr, "Line %lu: the snippet <%s> not defined!\n",
                                                                z_token->line_number, z_token->content->body);
                                                fprintf(output, "%s%s%s",
                                                                symbols->snippet_reference_start_mark->body,
                                                                z_token->content->body,
                                                                symbols->snippet_reference_stop_mark->body);
                                                exit(EXIT_FAILURE);
                                        }
                                        XITie *tie = wk_box_get(tie_box, XITie *);
                                        if (fmt->snippet_reference->n > 0) {
                                                WKArray *segments = snippet_name_split(z_token->content,
                                                                                       symbols->snippet_name_continuation);
                                                if (fmt->snippet_reference_start->n > 0) {
                                                        fprintf(output, "%s", fmt->snippet_reference_start->body);
                                                } else {
                                                        fprintf(output, "%s", symbols->snippet_reference_start_mark->body);
                                                }
                                                for (size_t j = 0; j < segments->n; j++) {
                                                        WKStr *segment = wk_array_get(segments, j, WKStr *);
                                                        if (j % 2 == 0) {
                                                                WKStr *snippet_name_t = wk_str(fmt->snippet_reference->body);
                                                                wk_str_replace(snippet_name_t, "${name}", segment->body);
                                                                fprintf(output, "%s", snippet_name_t->body);
                                                                wk_str_free(snippet_name_t);
                                                        } else { /* 输出续行符以及下一行的前导空白字符 */
                                                                if (fmt->snippet_name_continuation->n > 0) {
                                                                        wk_str_replace(segment,
                                                                                       symbols->snippet_name_continuation->body,
                                                                                       fmt->snippet_name_continuation->body);
                                                                        fprintf(output, "%s", segment->body);
                                                                } else {
                                                                        fprintf(output, "%s", segment->body);
                                                                }
                                                        }
                                                        wk_str_free(segment);
                                                }
                                                wk_array_free(segments);
                                                if (fmt->snippet_reference_stop->n > 0) {
                                                        fprintf(output, "%s", fmt->snippet_reference_stop->body);
                                                } else {
                                                        fprintf(output, "%s", symbols->snippet_reference_stop_mark->body);
                                                }
                                                for (size_t k = 0; k < tie->spatial_order->n; k++) {
                                                        WKBranch *z_ref = wk_array_get(tie->spatial_order, k, WKBranch *);
                                                        XISyntax *z_ref_syntax = wk_branch_get(z_ref, XISyntax *);
                                                        wk_str_printf(id_text, "%lu", z_ref_syntax->id);
                                                        if (fmt->snippet_reference_id->n > 0) {
                                                                WKStr *ref_id = wk_str(fmt->snippet_reference_id->body);
                                                                wk_str_replace(ref_id, "${id}", id_text->body);
                                                                fprintf(output, "%s", ref_id->body);
                                                                wk_str_free(ref_id);
                                                        }
                                                }
                                        } else {
                                                fprintf(output, "%s%s%s",
                                                                 symbols->snippet_reference_start_mark->body,
                                                                 z_token->content->body,
                                                                 symbols->snippet_reference_stop_mark->body);
                                                for (size_t k = 0; k < tie->spatial_order->n; k++) {
                                                        WKBranch *z_ref = wk_array_get(tie->spatial_order, k, WKBranch *);
                                                        XISyntax *z_ref_syntax = wk_branch_get(z_ref, XISyntax *);
                                                        fprintf(output, " <%lu>", z_ref_syntax->id);
                                                }
                                        }
                                }
                        }
                        break;
                default: ;
                }
        }
        WKStr *x_key = compact_text(x_name);
        WKBox *tie_box = wk_table_query(relations, wk_box_ref(x_key, WKStr *));
        if (tie_box) {
                XITie *tie = wk_box_get(tie_box, XITie *);
                if (tie->emissions->n > 0) {
                        for (size_t i = 0; i < tie->emissions->n; i++) {
                                WKBranch *e = wk_array_get(tie->emissions, i, WKBranch *);
                                XISyntax *e_syntax = wk_branch_get(e, XISyntax *);
                                WKBranch *e_name_branch = wk_array_get(e->lower, 0, WKBranch *);
                                XISyntax *e_name = wk_branch_get(e_name_branch, XISyntax *);
                                XIToken *e_name_token = wk_link_get(e_name->tokens->head, XIToken *);
                                wk_str_printf(id_text, "%lu", e_syntax->id);
                                if (fmt->snippet_emission->n > 0) {
                                        WKStr *e_fmt = wk_str(fmt->snippet_emission->body);
                                        wk_str_replace(e_fmt,
                                                       "${name}",
                                                       e_name_token->content->body);
                                        wk_str_replace(e_fmt, "${id}", id_text->body);
                                        fprintf(output, "%s", e_fmt->body);
                                        wk_str_free(e_fmt);
                                } else {
                                        fprintf(output, "=> %s <%s>\n",
                                                         e_name_token->content->body,
                                                         id_text->body);
                                        
                                }
                        }
                }
        }
        wk_str_free(x_key);
        do {
                if (fmt->snippet_stop->n > 0) {
                        XIToken *language = find_language_token(x);
                        WKStr *a = wk_str(fmt->snippet_stop->body);
                        if (language) {
                                wk_str_replace(a, "${language}", language->content->body);
                        }
                        fprintf(output, "%s", a->body);
                        wk_str_free(a);
                }
                
        } while (0);
        wk_str_free(id_text);
}
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
int main(int argc, char **argv) {
        WKArg xi_args[] = {
                WK_ARG_TOGGLE("tangle", "t"),
                WK_ARG_TEXT("entrance", "e"),
                WK_ARG_TOGGLE("line", "l"),
                WK_ARG_TEXT("beginning", "B"),
                WK_ARG_TEXT("end", "E"),
                WK_ARG_TOGGLE("weave", "w"),
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
        const char *beginning_opt = xi_args[3].value.text;
        if (strcmp(beginning_opt, "") == 0) beginning_opt = NULL;
        const char *end_opt = xi_args[4].value.text;
        if (strcmp(end_opt, "") == 0) end_opt = NULL;
        bool weave_opt = xi_args[5].value.toggle;
        const char *xi_config_path = xi_args[6].value.text;
        if (strcmp(xi_config_path, "") == 0) xi_config_path = NULL;
        const char *output_opt = xi_args[7].value.text;
        if (strcmp(output_opt, "") == 0) output_opt = NULL;
        bool help_opt = xi_args[8].value.toggle;
        const char *src_file_path = xi_args[9].value.text;
        if (strcmp(src_file_path, "") == 0) src_file_path = NULL;
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
        if (help_opt) {
                fprintf(stdout, "COMMAND: xi [OPTION]... FILE\n\n");
                fprintf(stdout, "%-16s  %s\n", "--tangle, -t",   "extract code snippets.");
                fprintf(stdout, "%-16s  %s\n", "--entrance, -e", "specify the name of the code fragment to extract.");
                fprintf(stdout, "%-16s  %s\n", "--line, -l",     "enable line numbers corresponding to the extracted");
                fprintf(stdout, "%-16s  %s\n", "",                "code fragment in the literate program.");
                fprintf(stdout, "%-16s  %s\n", "--beginning",     "set the beginning region of the code fragment");
                fprintf(stdout, "%-16s  %s\n", "",                "extraction range.");
                fprintf(stdout, "%-16s  %s\n", "--end",           "set the ending region of the code fragment");
                fprintf(stdout, "%-16s  %s\n", "",                "extraction range.");
                fprintf(stdout, "%-16s  %s\n", "--weave, -w",     "build the typeset documentation of the literate");
                fprintf(stdout, "%-16s  %s\n", "",                "program.");
                fprintf(stdout, "%-16s  %s\n", "--config, -c",   "specify the path to the xi configuration file.");
                fprintf(stdout, "%-16s  %s\n", "--output, -o",   "set the output file name or path for xi.");
                fprintf(stdout, "%-16s  %s\n", "--help, -h",     "display this help and exit.");
        } else {
                XISymbols *xi_symbols = malloc(sizeof(XISymbols));
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
                XIFmt *xi_fmt = malloc(sizeof(XIFmt));
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
                FILE *src_file = fopen(src_file_path, "r");
                if (!src_file) {
                	fprintf(stderr, "Failed to open %s\n", src_file_path);
                	exit(EXIT_FAILURE);
                }
                WKList *xi_tokens = xi_lexer(src_file, xi_symbols);
                WKTree *xi_tree = xi_parser(xi_tokens);
                skipped_chars_in_snippet_name = wk_array(const char *);
                wk_array_add(skipped_chars_in_snippet_name, " ", const char *);
                wk_array_add(skipped_chars_in_snippet_name, "　", const char *);
                wk_array_add(skipped_chars_in_snippet_name, "\t", const char *);
                wk_array_add(skipped_chars_in_snippet_name, "\n", const char *);
                wk_array_add(skipped_chars_in_snippet_name,
                             xi_symbols->snippet_name_continuation->body,
                             const char *);
                wk_free_bus_connect("XITie *", xi_tie_box_free);
                WKTable *relations = xi_create_relations(xi_tree->root);
                if (tangle_opt) {
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
                                fprintf(stderr, "Illegal output!");
                                exit(EXIT_FAILURE);
                        }
                        WKPair *area = snippet_area(xi_tree, beginning_opt, end_opt);
                        size_t u = wk_box_get(area->x, size_t);
                        size_t v = wk_box_get(area->y, size_t);
                        WKList *indents = wk_list(WKStr *);
                        for (size_t i = 0; i < entrances->n; i++) {
                                WKStr *entrance = wk_array_get(entrances, i, WKStr *);
                                WKStr *output_path = wk_array_get(outputs, i, WKStr *);
                                FILE *output = fopen(output_path->body, "w");
                                if (!output) {
                                        fprintf(stderr, "Failed to open %s!\n", output_path->body);
                                        exit(EXIT_FAILURE);
                                }
                                xi_tangle(src_file_path, relations,
                                          entrance, u, v, indents, line_opt, output);
                                fclose(output);
                        }
                        for (WKLink *it = indents->head; it; it = it->next) {
                                WKStr *indent = wk_link_get(it, WKStr *);
                                wk_str_free(indent);
                        }
                        wk_list_free(indents);
                        wk_box_free(area->x);
                        wk_box_free(area->y);
                        wk_pair_free(area);
                        for (size_t i = 0; i < entrances->n; i++) {
                                wk_str_free(wk_array_get(entrances, i, WKStr *));
                                wk_str_free(wk_array_get(outputs, i, WKStr *));
                        }
                        wk_array_free(entrances);
                        wk_array_free(outputs);
                }
                if (weave_opt) {
                        FILE *xi_output = fopen(output_opt, "w");
                        spread_language_mark(xi_tree, relations);
                        for (size_t i = 0; i < xi_tree->root->lower->n; i++) {
                                WKBranch *x = wk_array_get(xi_tree->root->lower, i, WKBranch *);
                                XISyntax *x_syntax = wk_branch_get(x, XISyntax *);
                                if (x_syntax->type == XI_SNIPPET) {
                                        XIToken *x_token = wk_link_get(x_syntax->tokens->head, XIToken *);
                                        fprintf(xi_output, "%s", x_token->content->body);
                                } else output_snippet_with_name(x, relations, xi_symbols, xi_fmt, xi_output);
                        }
                        fclose(xi_output);
                }
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
                fclose(src_file);
                for (size_t i = 0; i < skipped_chars->n; i++) {
                        WKStr *s = wk_array_get(skipped_chars, i, WKStr *);
                        wk_str_free(s);
                }
                wk_array_free(skipped_chars);
                wk_array_free(skipped_chars_in_snippet_name);
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
                wk_table_free(relations);
                delete_syntax_tree(xi_tree);
        }
        return 0;
}
