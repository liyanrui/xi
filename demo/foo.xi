经典的 C 语言入门程序：

@ hello world # [C]
#include <stdio.h>
int main(void) {
        printf("hello world!\n");
        return 0;
}
@

假设上述代码保存在 hello.c 文件，可使用以下命令编译该文件，获得程序 hello：

<pre>
$ gcc hello.c -o hello
</pre>