# 简介

所谓文学编程，其理念是，容易让人理解的程序应当以撰写一篇文章甚至一本书的方式去编写。一个程序可以视为是一栋建筑，通过文字去描述它，能够给人快速领会其本质的渠道或机会，而不至于陷入到程序源码的具体细节里，管中窥豹，甚至盲人摸象。程序的源码可以打碎，分散在文字所描述的各处情节之中，如同一张又一张图纸，精确描画这栋建筑，最终我们得到的是可以排版印刷供人阅读的文档。若需要程序在计算机里运行，可以通过一个工具，将程序的源码从文档里完整提取出来，交由编译器生成具体程序，而 xi 便是这样的一个工具。

# 安装

假设在 Linux 环境里安装 xi，由于 xi 依赖 WK 库（<https://github.com/liyanrui/wk>），故而在安装 xi 之前，需要先行下载并编译 WK 库：

```console
$ git clone https://github.com/liyanrui/wk.git
$ cd wk
$ make
$ sudo make install
```

WK 库文件 libwk.a 与 libwk.so 默认会安装在 /usr/local/bin 目录，头文件则在 /usr/local/include 目录。

然后下载 xi 源码，以连接 WK 静态库（libwk.a）的方式编译它：

```console
$ git clone https://github.com/liyanrui/xi.git
$ cd xi
$ gcc xi.c -Wl,-Bstatic -lwk -Wl,-Bdynamic -o xi
```

倘若用连接 WK 共享库的方式编译 xi.c，亦即

```console
$ gcc xi.c -lwk -o xi
```

那么在运行所得 xi 程序时，你的 Linux 系统可能需要你在 Shell 配置文件里设定共享库搜索路径。例如，对于 Bash Shell 而言，需要在 $HOME/.bashrc 文件里添加以下内容：

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

然后重新开启 Shell，方能在运行 xi 程序时找到 libwk.so。倘若是用连接 WK 静态库的方式编译 xi，则无需如此麻烦。

将所得 xi 程序安装到 /usr/local/bin 目录：

```console
$ sudo install xi /usr/local/bin
```

# 用法

假设有以下文学程序 hello.xi：

```
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
```

使用以下命令可从 foo.xi 中提取名字为「hello world」代码片段里的全部代码：

```console
$ xi --tangle --entrance "hello world" --output hello.c foo.xi
```

或者

```console
$ xi -t -e "hello world" -o hello.c foo.xi
```

使用以下命令可将 foo.xi 转为特定格式的文档，例如 Markdown 格式：

```console
$ xi --weave --config markdown.conf --output foo.md foo.xi
```

或者

```console
$ xi -w -c markdown.conf -o foo.md foo.xi
```

markdown.conf 文件可以放在 foo.xi 所在目录，其内容为

```yaml
snippet_start: <pre>
snippet_stop: </pre>
snippet_name: "\n<span class=\"snippet-name\" id=\"#xi-${id}\">@${name}#</span> "
snippet_reference: "<span class=\"snippet-reference\">#${name}@</span>"
snippet_reference_id: "<span class=\"snippet-id\"><a href=\"#xi-${id}\">[${id}]</a></span>"
snippet_emission: "<span class=\"snippet-emission\">=> ${name}</span>
                   <span class=\"snippet-id\">
                      <a href=\"#xi-${id}\">[${id}]</a>
                   </span>"
```

上述命令生成的 foo.md 的内容如下：

```markdown
C 语言的 hello world 程序可以像下面这样逐步写出来。首先，需要包含标准库中的标准输入输出头文件：

<pre>
<span class="snippet-name" id="#xi-1">@ hello world #</span>
#include <stdio.h>
</pre>

然后，编写程序的入口函数：

<pre>
<span class="snippet-name" id="#xi-2">@ hello world #</span> +
int main(void) {
        <span class="snippet-reference"># 在屏幕上打印 Hello world! @</span><span class="snippet-id"><a href="#xi-3">[3]</a></span>
        return 0;
}
</pre>

最后，调用标准库的 `printf` 函数在屏幕上打印字符串 `"Hello world!"`：

<pre>
<span class="snippet-name" id="#xi-3">@ 在屏幕上打印 Hello world! #</span>
printf("Hello world!\n");
<span class="snippet-emission">=>  hello world </span><span class="snippet-id"><a href="#xi-2">[2]</a></span></pre>
```

# 配置文件

理论上，xi 能够支持任何排版语言，前提是需要提供相应的格式化配置文件。下面是我为 ConTeXt LMTX 编写的配置文件，谨供参考：

```yaml
snippet_start: \start${language}
snippet_stop: \stop${language}
snippet_name: "\n/BTEX\color[darkred]{@${name}\#}\reference[xi-${id}]{${id}}\inoutermargin{\darkred{${id}}}/ETEX"
snippet_tag: "\n/BTEX\color[darkmagenta]{<${name}>}/ETEX"
snippet_tag_reference: "/BTEX\color[darkmagenta]{\ <${name}>\ }/ETEX"
snippet_reference: "/BTEX\color[darkcyan]{\tt \#${name}@}/ETEX"
snippet_reference_id: "/BTEX\ <\in[xi-${id}]>/ETEX"
snippet_emission: "/BTEX=>\color[darkyellow]{${name}}<\in[xi-${id}]>/ETEX\n"
```
