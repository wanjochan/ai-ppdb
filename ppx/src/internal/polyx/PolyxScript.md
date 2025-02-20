# PolyxScript 设计文档

## 1. 概述
PolyxScript 是一个为 PPX 设计的轻量级脚本语言，采用类 LISP 的前缀表达式风格，主要用于系统集成和数据流处理。

## 2. 设计目标
- 简单直观的语法
- 强大的系统集成能力
- 灵活的数据流处理
- 完备的异步支持
- 统一的表达式形式

## 3. 核心语法

### 3.1 基础动词
```lisp
// 变量绑定和赋值
let(x, 42)
let(y, +(x, 10))

// 函数定义
let(add,
    fn((a, b),
       expr(+(a, b))))

// 表达式块
expr(
    let(x, 1),
    let(y, 2),
    +(x, y)
)

// 返回值
return(value)
```

### 3.2 控制流
```lisp
// 条件判断
if(=(x, 10),
   expr(print("equal")),
   expr(print("not equal")))

// 循环
while(<=(x, 10),
      expr(
         print(x),
         let(x, +(x, 1))
      ))

// 迭代
for(i, range(0, 10),
    expr(print(i)))
```

### 3.3 系统集成
```lisp
// 数据流管道
pipe(
    getData(),
    expr(filter(it, >(it.value, 10))),
    expr(map(it, *(it.value, 2))),
    expr(reduce(it, 0, +(acc, it)))
)

// 系统回调绑定
bind("onDataChange",
     fn((data),
        expr(
            let(processed, process(data)),
            notify(processed)
        )))

// 异步操作
async(expr(
    let(data, await(fetchData())),
    let(result, process(data)),
    return(result)
))
```

## 4. 关键字列表

### 4.1 基础关键字
- `let` - 变量绑定和赋值
- `fn` - 函数定义
- `expr` - 表达式块
- `return` - 返回值

### 4.2 控制流关键字
- `if` - 条件分支
- `while` - 循环控制
- `for` - 迭代控制

### 4.3 系统集成关键字
- `pipe` - 管道操作
- `bind` - 回调绑定
- `async` - 异步操作
- `await` - 等待异步结果

## 5. 运算符
所有运算符都作为函数使用：
- `+`, `-`, `*`, `/` - 算术运算
- `=`, `!=`, `<`, `>`, `<=`, `>=` - 比较运算
- `and`, `or`, `not` - 逻辑运算

## 6. 实现注意事项

### 6.1 解析器实现
- 采用递归下降解析
- AST 基于表达式树
- 支持语法错误恢复
- 提供详细的错误位置信息

### 6.2 类型系统
- 动态类型系统
- 基本类型：数值、字符串、布尔、nil
- 复合类型：列表、函数
- 运行时类型检查

### 6.3 闭包实现
- 词法作用域
- 变量捕获
- 函数作为一等公民
- 支持高阶函数

### 6.4 异步机制
- 基于事件循环的异步执行
- Promise 风格的异步操作
- 异步函数自动调度
- 支持取消和超时

### 6.5 错误处理
- 异常传播机制
- try/catch 支持
- 异步错误处理
- 错误堆栈跟踪

### 6.6 内存管理
- 引用计数
- 循环引用检测
- 自动内存回收
- 资源自动释放

## 7. 示例

### 7.1 异步文件处理
```lisp
let(processFile,
    fn((path),
       async(expr(
           try(
               expr(
                   let(content, await(readFile(path))),
                   let(processed, process(content)),
                   await(writeFile(path, processed))
               ),
               fn((err),
                  expr(log(err)))
           )
       ))))
```

### 7.2 事件处理
```lisp
bind("dataStream",
     fn((event),
        expr(
            let(value, event.data),
            if(>(value, threshold),
               expr(
                   async(notify("high-value", value)),
                   updateMetrics(value)
               ),
               expr(log("normal-value", value)))
        )))
```

### 7.3 数据流转换
```lisp
let(transform,
    fn((data),
       pipe(
           data,
           expr(filter(it, not(isNull(it)))),
           expr(map(it, normalize(it))),
           expr(groupBy(it, it.category)),
           expr(reduce(it, {}, merge(acc, it)))
       )))
```