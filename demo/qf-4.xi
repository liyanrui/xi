\environment xi-style
\starttext
\title{青凤}

先大父尝植柏于后园，其干虬曲，其叶蓊郁。余年幼，每值月明，辄见一女立树下，衣青如叶，面白如月。问名不答，笑而已。后数年不复见。

@ 外景 # [C]
#include <stdio.h>
#include <stdlib.h>
@

及冠，于故纸堆中得《青凤斋笔记》，纸焦墨淡，字迹如烟。卷首便是这两个奇怪的符号行，非篆非隶，似图似符。笔记云：青凤斋非寻常屋舍，其庭制若树，根深而枝分，能自调疏密，虽增删不止，恒平如砥。

@ 庭院 #
typedef struct Node {
    int key;
    struct Node *left;
    struct Node *right;
    int height;
} Node;
@

笔记又云，青凤每夕月下起舞。其舞有根基二则，一曰“知高”，一曰“明较”。知高者，察廊柱之高下也；明较者，衡左右之轻重也。

@ 求高与取大 #
int height(Node *n) {
    if (n == NULL) return 0;
    return n->height;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}
@

舞姿有四。其一曰“左揽”。若左廊偏坠，则以右足为枢，身微左倾，虚引其重，归于平衡。月下见之，如揽月入怀，衣袂翻飞。

@ 左揽 #
Node *rotate_right(Node *y) {
    Node *x = y->left;
    Node *T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;
}
@

其二曰“右牵”。若右侧过沉，则以左足为枢，身往右引。其势如牵流云出岫，环佩微动而庭院肃然。

@ 右牵 #
Node *rotate_left(Node *x) {
    Node *y = x->right;
    Node *T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;

    return y;
}
@

其三曰“安宅”。有新客投止，青凤必亲引之，视其轻重，置诸一隅。既置，乃遍踏诸院，遇偏则旋，直至四方匀停。余尝见其安一客而三旋其身，左揽右牵，连环迭用，如风回雪。

@ 安宅 #
Node *insert(Node *node, int key) {
    if (node == NULL) {
        Node *new_node = (Node *)malloc(sizeof(Node));
        new_node->key = key;
        new_node->left = NULL;
        new_node->right = NULL;
        new_node->height = 1;
        return new_node;
    }

    if (key < node->key)
        node->left = insert(node->left, key);
    else if (key > node->key)
        node->right = insert(node->right, key);
    else
        return node;

    node->height = 1 + max(height(node->left), height(node->right));

    int balance = height(node->left) - height(node->right);

    if (balance > 1 && key < node->left->key)
        return rotate_right(node);

    if (balance < -1 && key > node->right->key)
        return rotate_left(node);

    if (balance > 1 && key > node->left->key) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    if (balance < -1 && key < node->right->key) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}
@

其四曰“游观”。某夕，青凤忽邀余遍览诸院。但见廊腰缦回，檐牙高啄，所历次序，井然不紊。先左偏院，次夹道，次中亭，次右长廊。其间有数，小者在前，大者在后，无一紊乱。

@ 游观 #
void inorder(Node *root) {
    if (root != NULL) {
        inorder(root->left);
        printf("%d ", root->key);
        inorder(root->right);
    }
}
@

笔记尾页，字迹潦草，似远行前匆匆所留：“余将去此，斋留后人。守衡之法，尽在符中。”其后附有一段散庭归寂的仪轨。

@ 归去 #
void free_tree(Node *root) {
    if (root != NULL) {
        free_tree(root->left);
        free_tree(root->right);
        free(root);
    }
}
@

余乃取素纸，将笔记中所有符记依序誊录，缀为一篇。其辞不能尽解，而诵读之时，辄觉庭树婆娑，月影参差，青凤似在左右。

@ 青凤斋记 #
# 外景 @
# 庭院 @
# 求高与取大 @
# 左揽 @
# 右牵 @
# 安宅 @
# 游观 @
# 归去 @

int main() {
    Node *root = NULL;
    int keys[] = {30, 20, 40, 10, 25, 35};
    int n = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < n; i++) {
        root = insert(root, keys[i]);
    }

    printf("青凤斋游观次序：");
    inorder(root);
    printf("\n");

    free_tree(root);
    return 0;
}
@

异史氏曰：世有奇术，托于怪文。得其文者，或见其符，或见其事，或二者俱见。余不能辨其然，但知月下有人，庭中有树，符在则舞不歇。

\stoptext