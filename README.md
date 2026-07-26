# 简介

所谓文学编程，其理念是，容易让人理解的程序应当以撰写一篇文章甚至一本书的方式去编写。程序的源码可以打碎，分散在文字所描述的各处情节之中，如同一张又一张图纸，精确描画这栋建筑，最终我们得到的是可以排版印刷供人阅读的文档。若需要程序在计算机里运行，便将程序的源码从文档里完整提取出来，交由编译器生成具体程序。xi 便是支持这种编程方式的一个简单的工具。

# 编译

Xi 依赖 WK 库（<https://github.com/liyanrui/wk>）。假设在 Linux 环境里安装 xi，需要先行下载、编译和安装 WK 库：

```console
$ git clone https://github.com/liyanrui/wk.git
$ cd wk
$ make
$ sudo make install
```

WK 库文件 libwk.a 与 libwk.so 默认会安装在 /usr/local/bin 目录，头文件则在 /usr/local/include 目录。

然后下载 xi 源码，建议以连接 WK 静态库（libwk.a）的方式编译：

```console
$ git clone https://github.com/liyanrui/xi.git
$ cd xi
$ gcc xi.c -lwk -o xi
```

注意，在运行所得 xi 程序时，你的 Linux 系统可能需要你在 Shell 配置文件里设定共享库搜索路径。例如，对于 Bash Shell，可在 $HOME/.bashrc 文件里添加以下内容：

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

然后重新启动 Shell，运行 xi 程序时，系统便可搜到并加载 libwk.so。

倘若不想配置共享库路径，也可以考虑以连接 WK 静态库（libwk.a）的方式编译 xi：

```console
$ gcc xi.c -Wl,-Bstatic -lwk -Wl,-Bdynamic -o xi
```

# 抽取源码

假设文学程序文件 foo.xi，其内容如下：

```
以下函数用于计算向量点积：

@ 点积函数 # [C]
int dot_product(double x[], double y[], int n) {
        double sum = 0;
        # 计算 n 维向量 x 与 y 的点积 @
        return sum;
}
@

n 维向量点积计算过程如下：

@ 计算 n 维向量 x 与 y 的点积 #
for (i = 0; i < n; i++) {
        sum += x[i] * y[i];
}
@
```

使用以下命令，可抽取 `点积函数` 片段的全部代码并保存于 foo.c 文件：

```console
$ xi --tangle --entrance "点积函数" --output foo.c foo.xi
```

或者

```console
$ xi -t -e "点积函数" -o foo.c foo.xi
```

foo.c 文件的内容如下：

```c
int dot_product(double x[], double y[], int n) {
        double sum = 0;
        for (i = 0; i < n; i++) {
                sum += x[i] * y[i];
        }
        return sum;
}
```

# 编织文档

若将上一节 foo.xi 文件转换为 Markdown 或 HTML 格式的文档，但 xi 并不知晓如何将文学编程标记转化为相应的格式，它需要我们以配置文件的方式告诉它应该如何做。下面是一份简单的 xi 配置文件markdown.conf 的内容：

```yaml
snippet_start: "<pre>\n"
snippet_stop: "</pre>"
snippet_name: "<span class=\"snippet-name\">${name}</span>"
snippet_id: "<span class=\"snippet-id\" id=\"#xi-${id}\">&lt;${id}&gt;</span>"
snippet_reference: "<span class=\"snippet-reference\">${name}</span>"
snippet_reference_id: "<span class=\"snippet-id\">
                         <a href=\"#xi-${id}\">&lt;${id}&gt;</a>
                       </span>"
snippet_emission: "<span class=\"snippet-emission\">=> ${name}</span>
                   <span class=\"snippet-id\">
                      <a href=\"#xi-${id}\">&lt;${id}&gt;</a>
                   </span>"
```

基于 markdown.conf 文件，可将 foo.xi 转化为 Markdown 文档 foo.md 的命令为

```
$ xi --weave --config html.conf --output foo.html foo.xi
```

或者

```
$ xi -w -c html.conf -o foo.html foo.xi
```

生成的 foo.md 内容如下：

```html
以下函数用于计算向量点积：

<pre>
@<span class="snippet-name"> 点积函数 </span>#<span class="snippet-id" id="#xi-1">&lt;1&gt;</span>
int dot_product(double x[], double y[], int n) {
        double sum = 0;
        #<span class="snippet-reference"> n 维向量 x 与 y 的点积 </span>@<span class="snippet-id"><a href="#xi-2">&lt;2&gt;</a></span>
        return sum;
}
</pre>

n 维向量点积计算过程如下：

<pre>
@<span class="snippet-name"> n 维向量 x 与 y 的点积 </span>#<span class="snippet-id" id="#xi-2">&lt;2&gt;</span>
for (i = 0; i < n; i++) {
        sum += x[i] * y[i];
}
<span class="snippet-emission">=>  点积函数 </span><span class="snippet-id"><a href="#xi-1">&lt;1&gt;</a></span></pre>
```

Xi 支持的所有格式化配置如下：

* `snippet_start`：片段开始标记，可用占位符 `${language}`；
* `snippet_stop`：片段结束标记，可用占位符 `${language}`；
* `snippet_name_start`：片段名开始标记；
* `snippet_name`：片段名标记，可用占位符 `${name}`；
* `snippet_id`：片段 ID，可用占位符 `${id}`；
* `snippet_name_stop`：片段名终止标记；
* `snippet_name_continuation`：片段名内续行符标记；
* `snippet_tag`：片段标签，可用占位符 `${id}` 和 `${name}`，注意此处 `id` 是片段 ID，但 `name` 是标签名字；
* `snippet_tag_reference`：片段标签引用，可用占位符 `${id}` 和 `${name}`，注意此处 `id` 是片段 `id`，但 `name` 是标签名字；
* `snippet_reference_start`：片段引用开始标记；
* `snippet_reference`：片段引用标记，可用占位符 `${name}`；
* `snippet_reference_id`：片段引用 id 标记，可用占位符 `${id}`；
* `snippet_reference_stop`：片段引用终止标记；
* `snippet_emission`：片段引用者标记，可用占位符 `${id}` 和 `${name}`；
* `snippet_appending_operator`：片段合并运算符标记；
* `snippet_prepending_operator`：片段前向合并运算符标记。

# YAML

倘若你想更为精细地操控 xi 的文档编织过程，可基于 xi 输出的 YAML 格式数据实现这一目的。以下命令可将 foo.xi 转化为 YAML 文档 foo.yml：

```
$ xi --weave --yaml --output foo.yml foo.xi
```

或者

```
$ xi -w -y -o foo.yml foo.xi
```

foo.yml 内容如下：

```yaml
- type: no-name
  content: |-
    '以下函数用于计算向量点积：
    
    '
- type: with-name
  name: |-
    ' 点积函数 '
  id: 1
  language: C
  content:
    - type: text
      data: |-
        '
        int dot_product(double x[], double y[], int n) {
                double sum = 0;
                '
  content:
    - type: snippet-reference
      data:
      name: |-
        ' n 维向量 x 与 y 的点积 '
      ids:
        - 2
  content:
    - type: text
      data: |-
        '
                return sum;
        }
        '
- type: no-name
  content: |-
    '
    
    n 维向量点积计算过程如下：
    
    '
- type: with-name
  name: |-
    ' n 维向量 x 与 y 的点积 '
  id: 2
  language: C
  content:
    - type: text
      data: |-
        '
        for (i = 0; i < n; i++) {
                sum += x[i] * y[i];
        }
        '
  emissions:
    - name: |-
        ' 点积函数 '
      id: 1
```

# 文档

Xi 程序本身便是以 xi 所支持的文学编程方式实现的，程序源码文件 xi.c 便是从其文学程序文件的源码抽取结果，但是 xi 的文学程序及其文档，目前我还不舍的公诸于众。
