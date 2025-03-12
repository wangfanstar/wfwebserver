midoc 的 sqlite3 数据库文件mindoc.db中 表有以下2个
提供一个linux c 程序可以将以下数据库中表的信息按文档数量排名生成html网页

信息包括 account real_name document_name
CREATE TABLE `md_documents` (
    `document_id` integer NOT NULL PRIMARY KEY AUTOINCREMENT,
    `document_name` varchar(500) NOT NULL DEFAULT '' ,
    `identify` varchar(100) DEFAULT 'null' ,
    `book_id` integer NOT NULL DEFAULT 0 ,
    `parent_id` integer NOT NULL DEFAULT 0 ,
    `order_sort` integer NOT NULL DEFAULT 0 ,
    `markdown` text,
    `release` text,
    `content` text,
    `create_time` datetime NOT NULL,
    `member_id` integer NOT NULL DEFAULT 0 ,
    `modify_time` datetime NOT NULL,
    `modify_at` integer NOT NULL DEFAULT 0 ,
    `version` integer NOT NULL DEFAULT 0 ,
    `is_open` integer NOT NULL DEFAULT 0 ,
    `view_count` integer NOT NULL DEFAULT 0 ,
    UNIQUE (`book_id`, `identify`)
)

CREATE TABLE `md_members` (
    `member_id` integer NOT NULL PRIMARY KEY AUTOINCREMENT,
    `account` varchar(100) NOT NULL DEFAULT ''  UNIQUE,
    `real_name` varchar(255) NOT NULL DEFAULT '' ,
    `password` varchar(1000) NOT NULL DEFAULT '' ,
    `auth_method` varchar(50) NOT NULL DEFAULT 'local' ,
    `description` varchar(2000) NOT NULL DEFAULT '' ,
    `email` varchar(100) NOT NULL DEFAULT ''  UNIQUE,
    `phone` varchar(255) DEFAULT 'null' ,
    `avatar` varchar(1000) NOT NULL DEFAULT '' ,
    `role` integer NOT NULL DEFAULT 1 ,
    `status` integer NOT NULL DEFAULT 0 ,
    `create_time` datetime NOT NULL,
    `create_at` integer NOT NULL DEFAULT 0 ,
    `last_login_time` datetime
)


CREATE TABLE `md_books` (
    `book_id` integer NOT NULL PRIMARY KEY AUTOINCREMENT,
    `book_name` varchar(500) NOT NULL DEFAULT '' ,
    `item_id` integer NOT NULL DEFAULT 1 ,
    `identify` varchar(100) NOT NULL DEFAULT ''  UNIQUE,
    `auto_release` integer NOT NULL DEFAULT 0 ,
    `is_download` integer NOT NULL DEFAULT 0 ,
    `order_index` integer NOT NULL DEFAULT 0 ,
    `description` varchar(2000) NOT NULL DEFAULT '' ,
    `publisher` varchar(500) NOT NULL DEFAULT '' ,
    `label` varchar(500) NOT NULL DEFAULT '' ,
    `privately_owned` integer NOT NULL DEFAULT 0 ,
    `private_token` varchar(500),
    `book_password` varchar(500),
    `status` integer NOT NULL DEFAULT 0 ,
    `editor` varchar(50) NOT NULL DEFAULT '' ,
    `doc_count` integer NOT NULL DEFAULT 0 ,
    `comment_status` varchar(20) NOT NULL DEFAULT 'open' ,
    `comment_count` integer NOT NULL DEFAULT 0 ,
    `cover` varchar(1000) NOT NULL DEFAULT '' ,
    `theme` varchar(255) NOT NULL DEFAULT 'default' ,
    `create_time` datetime NOT NULL,
    `history_count` integer NOT NULL DEFAULT 0 ,
    `is_enable_share` integer NOT NULL DEFAULT 0 ,
    `member_id` integer NOT NULL DEFAULT 0 ,
    `modify_time` datetime,
    `version` integer NOT NULL DEFAULT 0 ,
    `is_use_first_document` integer NOT NULL DEFAULT 0 ,
    `auto_save` integer NOT NULL DEFAULT 0 
)

提供以下内容

1、最新更新的文档（默认打开显示的视图）
按日期最新更新的100篇文档，文档链接，文档作者帐号，文档作者姓名, 文档最后更新时间

2、最受欢迎文档
按所有时间，过去一个月，过去一周提供view_count排名的100篇文档，文档链接，文档作者帐号，文档作者姓名, 文档最后更新时间

3、个人文档排名
分3部分显示，所有时间，过去一个月，过去一周
显示 排名 帐号  姓名  文档数量   文档列表（默认折叠，点击后显示详细文档带链接） 

4、组内文档排名
分2部分显示 指定配置组文档排名 和 所有组文档排名
指定配置组文档排名只显示程序同文件夹下的配置文件config.ini中sort_books中的书籍名称进行排名
book的排名显示成“组内文档排名”
显示样式为
排名  book名称   文档数量   文档名称   相关作者(real_name 没有则显示帐号，有则显示 real_name)
配置文件如下所示：时间为分钟，html路径为生成的html文件名及路径


配置文件名为mindocsort.conf 内容如下所示，recycle_time 为更新时间，以10分钟为单位更新网页，db_path为mindoc的路径 html_path 为生成网页的路径
link_prefix 为定制文档链接的前缀, 文档链接为如下格式 “link_prefix/md_books.identify/md_documents.identify”

# MinDoc 统计排名生成器配置文件
# 更新周期(分钟)
recycle_time = 10
# 数据库路径  
db_path = /home/w12043/soft/mindoc_linux_amd64/database/database/mindoc.db  
# HTML输出路径  
html_path = /home/w12043/webserver/html/public/mindocsort.html 
# 要排名的书籍名称(逗号分隔)  
sort_books = 支撑1组FAQ, 支撑2组FAQ, L2组FAQ, QACL组FAQ, 转发FAQ, 业务组FAQ, 接口组FAQ, CP组FAQ
# 文档链接前缀
link_prefix = http://10.114.209.41:8181/docs

生成的网页优先显示最新更新的文档，上部固定导航按钮（翻页时固定在上部）可切换成最受欢迎文档，个人文档排名，组内文档排名(按组内文档数量排名),生成的html可引用同文件夹下的tailwind.min.css文件和alpine.min.js文件，美化下界面 提供linux上纯C实现的所有源代码


