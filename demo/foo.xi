C 语言的 hello world 程序可以像下面这样逐步写出来。首先，需要包含标准库中的标准输入输出头文件：

@ hello world # [C]
#include <stdio.h>
@

然后，编写程序的入口函数：

@ hello world # +
int main(void) {
        # 在屏幕上打印 Hello world! @
        return 0;
}
@

最后，调用标准库的 `printf` 函数在屏幕上打印字符串 `"Hello world!"`：

@ 在屏幕上打印 Hello world! #
printf("Hello world!\n");
@