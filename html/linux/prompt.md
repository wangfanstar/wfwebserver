<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Linux同步原语对比 - 互斥锁</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            display: flex;
        }
        .sidebar {
            width: 250px;
            background-color: #2c3e50;
            color: white;
            height: 100vh;
            position: fixed;
            overflow-y: auto;
        }
        .content {
            margin-left: 250px;
            padding: 20px;
            width: calc(100% - 250px);
        }
        .sidebar-header {
            padding: 20px;
            text-align: center;
            border-bottom: 1px solid #34495e;
        }
        .nav-link {
            display: block;
            padding: 15px 20px;
            color: #ecf0f1;
            text-decoration: none;
            transition: background-color 0.3s;
        }
        .nav-link:hover {
            background-color: #34495e;
        }
        .active {
            background-color: #3498db;
        }
        .card {
            background-color: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            margin-bottom: 20px;
            overflow: hidden;
        }
        .card-header {
            background-color: #3498db;
            color: white;
            padding: 15px 20px;
            font-weight: bold;
        }
        .card-body {
            padding: 20px;
        }
        .note-box {
            background-color: #f8f9fa;
            border-left: 4px solid #e74c3c;
            padding: 15px;
            margin: 20px 0;
            border-radius: 0 4px 4px 0;
        }
        code {
            background-color: #f8f9fa;
            padding: 2px 4px;
            border-radius: 4px;
            font-family: 'Courier New', Courier, monospace;
        }
        .code-block {
            background-color: #f8f9fa;
            padding: 15px;
            border-radius: 4px;
            font-family: 'Courier New', Courier, monospace;
            white-space: pre;
            overflow-x: auto;
            line-height: 1.5;
            margin: 15px 0;
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            border: 1px solid #ddd;
            padding: 8px 12px;
            text-align: left;
        }
        th {
            background-color: #f2f2f2;
        }
        tr:nth-child(even) {
            background-color: #f9f9f9;
        }
    </style>
</head>
<body class="bg-gray-100">
    <div class="sidebar">
        <div class="sidebar-header">
            <h2 class="text-xl font-bold">Linux同步原语对比</h2>
        </div>
        <nav>
            <a href="index.html" class="nav-link">首页</a>
            <a href="mutex.html" class="nav-link active">互斥锁 (Mutex)</a>
            <a href="rwlock.html" class="nav-link">读写锁 (RWLock)</a>
            <a href="spinlock.html" class="nav-link">自旋锁 (Spinlock)</a>
            <a href="atomic.html" class="nav-link">原子操作 (Atomic)</a>
            <a href="semaphore.html" class="nav-link">信号量 (Semaphore)</a>
            <a href="rcu.html" class="nav-link">RCU</a>
            <a href="seqlock.html" class="nav-link">顺序锁 (Seqlock)</a>
            <a href="barrier.html" class="nav-link">内存屏障 (Barrier)</a>
            <a href="rwbarrier.html" class="nav-link">读写屏障 (RW Barrier)</a>
        </nav>
    </div>

    <div class="content">

    </div>
<script>window.parent.postMessage({ action: "ready" }, "*"); 
 
window.console = new Proxy(console, {
  get(target, prop) {
    if (['log', 'warn', 'error'].includes(prop)) {
      return new Proxy(target[prop], {
        apply(fn, thisArg, args) {
          fn.apply(thisArg, args);
          window.parent.postMessage({ action: 'console', 
            type: prop, 
            args: args.map((arg) => {
              try {
                return JSON.stringify(arg).replace(/^["']|["']$/g, '');
              } catch (e) {
                return arg;
              }
            }) 
          }, '*');
        }
      });
    }
    return target[prop];
  }
});
</script></body>
</html> 用这个模板提供linux用户态和内核态锁和原子操作等同步原语相关API的对比（只给C语言，不要C++）包括引用头文件，api, 使用场景，注意事项，提供html网页格式方便查看，给出每个api所对应的头文件，每个项目比较页面有单独的链接。页面导航链接固定在最左侧方便随时跳转，用户态和内核态的对比修改成左右两边的表格对比方式，每个用户态和内核态的对比页面有一个链接，不是2个链接，美化下界面。对比时内容分左右两边，左边为用户态，右边为内核态，用dog250的风格总结其设计原理和思想在这个网页上。先提供读写屏障的内容