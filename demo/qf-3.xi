\environment xi-style
\starttext
\title{青凤}

先大父尝植柏于后园，其干虬曲，其叶蓊郁。余年幼，每值月明，辄见一女立树下，衣青如叶，面白如月。问名不答，笑而已。后数年不复见。

及冠，于故纸堆中得《青凤斋笔记》，纸焦墨淡，字迹如烟。开卷即见数行奇怪文字，非篆非隶，似图似符。卷首题曰：

@ 外景 # [C]
#include <stdio.h>
#include <stdlib.h>
@

\noindent 余不能尽解，然观其形，如门户洞开，纳天地于方寸。笔记云：青凤斋非寻常屋舍，其庭制若树，根深而枝分，能自调疏密，虽增删不止，恒平如砥。斋中一切物事，各有其位，其轻重高下，咸载于簿。簿录之法，亦以怪字书之：

@ 庭院 #
typedef struct Node {
    int key;
    struct Node *left;
    struct Node *right;
    int height;
} Node;
@

\noindent 余揣度再三，恍然有悟。此“Node”者，盖斋中一隅，或廊或院，各有轻重（key），左右通达，兼记高下（height）。青凤守衡之秘，尽萃于此。

笔记又录步法根基。其辞曰：欲行守衡，先明高下；欲知偏正，须较轻重。

@ 求高与取大 #
int height(Node *n) {
    if (n == NULL) return 0;
    return n->height;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}
@

\noindent 此二语甚朴，犹习武者先扎马步。余效之，觉胸中豁然有清气流转。

青凤每夕月下起舞，其姿有四。

其一曰“左揽”。若左廊偏坠，则以右足为枢，身微左倾，虚引其重，归于平衡。观其文字记录，竟如揽月入怀，姿态飘逸：

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

\noindent 余初读不解“rotate_right”何意，后见青凤舞时右袖拂空，始悟乃右旋之谓。左揽者，右旋也，名实相乖，而妙理存焉。

其二曰“右牵”。若右侧过沉，则以左足为枢，身往右引，如牵流云出岫：

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

\noindent 余观此段，如见水袖回风，环佩微动。青凤之舞，非徒娱目，实乃调燮阴阳之术也。

其三曰“安宅”。有新客投止，青凤必亲引之，视其轻重，置诸一隅。既置，乃遍踏诸院，遇偏则旋，直至四方匀停。其法密如织锦：

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

\noindent 余细玩此段，乃知青凤之智，深不可测。每安一客，必循枝而下，如入幽谷；既至其所，复循枝而上，步步衡其偏正。其左揽右牵，连环迭用，一如阴阳消长，奇正相生。

其四曰“游观”。余尝从青凤遍览诸院。但见廊腰缦回，檐牙高啄，所历次序，井然不紊。后余默志其途，乃得一贯连之径：

@ 游观 #
void inorder(Node *root) {
    if (root != NULL) {
        inorder(root->left);
        printf("%d ", root->key);
        inorder(root->right);
    }
}
@

\noindent 自此径出，则满庭物事，小大有序，如展画轴。余始悟守衡之庭，非徒不倾，亦且便于游观。青凤之舞，实将乱丝理为锦缎也。

笔记尾页，字迹潦草，似青凤远行前匆匆所留：“余将去此，斋留后人。守衡之法，尽在符中。临别无他，唯以此诀相赠。”其下便是一段散庭归寂的仪轨：

@ 归去 #
void free_tree(Node *root) {
    if (root != NULL) {
        free_tree(root->left);
        free_tree(root->right);
        free(root);
    }
}
@

\noindent 余掩卷太息，怅然若失。乃取素纸，将笔记中所有奇怪文字依序誊录，缀为一篇。其辞如下：

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

\noindent 余每诵此篇，辄觉庭树婆娑，月影参差，青凤似在左右。其舞兮，其符兮，竟不可分。或谓：符者舞之骨，舞者符之魂。余不能辨，但知守衡之道，自此长存矣。

异史氏曰：世有奇术，托于怪文，得其文者，犹得术焉。然非有缘，虽对之而不识。余幸遇青凤，以舞传心，复以笔记留符。符在，则庭不倾，舞不歇。此岂非《志异》之遗意乎？

\stoptext