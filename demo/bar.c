#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
typedef struct word_node {
        char *word;           /* 词语本身 */
        int count;            /* 被拾起的次数 */
        struct word_node *next;
} WordNode;
WordNode *find_word(WordNode *head, const char *word) {
        WordNode *p = head;
        while (p) {
                if (strcmp(p->word, word) == 0)
                        return p;
                p = p->next;
        }
        return NULL;
}
WordNode *add_word(WordNode *head, const char *word) {
        WordNode *node = malloc(sizeof(WordNode));
        node->word = strdup(word);
        node->count = 1;
        node->next = head;
        return node;
}
int get_word(FILE *fp, char *buf, int buf_size) {
        int c;
        /* 跳过非字母数字字符 */
        while ((c = fgetc(fp)) != EOF) {
                if (isalnum(c))
                        break;
        }
        if (c == EOF) return 0;

        int i = 0;
        buf[i++] = tolower(c);
        while (i < buf_size - 1 && (c = fgetc(fp)) != EOF && isalnum(c)) {
                buf[i++] = tolower(c);
        }
        buf[i] = '\0';
        return 1;
}

int main(int argc, char *argv[]) {
        FILE *fp = stdin;
        if (argc > 1) {
                fp = fopen(argv[1], "r");
                if (!fp) {
                        fprintf(stderr, "无法打开田野 %s\n", argv[1]);
                        return 1;
                }
        }

        WordNode *head = NULL;
        char buf[256];

        while (get_word(fp, buf, sizeof(buf))) {
                WordNode *node = find_word(head, buf);
                if (node) {
                        node->count++;
                } else {
                        head = add_word(head, buf);
                }
        }

        if (fp != stdin) fclose(fp);

        /* 清点：找出出现次数最多的若干词语 */
        int top_n = 20;
        WordNode *p;
        for (int i = 0; i < top_n && head; i++) {
                WordNode *max_node = head;
                for (p = head->next; p; p = p->next) {
                        if (p->count > max_node->count)
                                max_node = p;
                }
                if (max_node->count == 0) break;
                printf("%s: %d\n", max_node->word, max_node->count);
                max_node->count = 0;
        }

        /* 释放布囊，离开田野 */
        while (head) {
                WordNode *next = head->next;
                free(head->word);
                free(head);
                head = next;
        }

        return 0;
}
