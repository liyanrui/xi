以下函数用于计算向量点积：

@ 点积函数 # [C]
int dot_product(double x[], double y[], int n) {
        double sum = 0;
        # n 维向量 x 与 y 的点积 @
        return sum;
}
@

n 维向量点积计算过程如下：

@ n 维向量 x 与 y 的点积 #
for (i = 0; i < n; i++) {
        sum += x[i] * y[i];
}
@