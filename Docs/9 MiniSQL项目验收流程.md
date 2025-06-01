# 9 MiniSQL项目验收流程



### 项目验收流程

1. 进行基本操作演示；
2. 简要介绍整个系统的设计思路，若实现了额外的功能或是性能优化可以一并介绍，介绍的时候最好能够体现出整体系统设计的亮点之处；
3. 模块提问抽查，针对一些实现细节做提问，考察项目是否是由小组独立完成的**（可能会问得非常深入，请做好充分准备）**

### 基本操作示例

1. 创建数据库`db0`、`db1`、`db2`，并列出所有的数据库
2. 在`db0`数据库上创建数据表`account`，表的定义如下：

```sql
create table account(
  id int, 
  name char(16) unique, 
  balance float, 
  primary key(id)
);

-- Note: 在实现中自动为UNIQUE列建立B+树索引的情况下，
--       这里的NAME列不加UNIQUE约束，UNIQUE约束将另行考察。
--			（NAME列创建索引的时候，不需要限制只有UNIQUE列才能建立索引）
```

1. 考察SQL执行以及数据插入操作：

1. 执行数据库文件`sql.txt`，向表中插入![img](https://cdn.nlark.com/yuque/__latex/d3df89690bb9f1eb2c7e46882ad9383f.svg)条记录（分![img](https://cdn.nlark.com/yuque/__latex/134c802fc5f0924cf1ea838feeca6c5e.svg)次插入，每次插入![img](https://cdn.nlark.com/yuque/__latex/db65a9f55418ccd1c30293cc83355e67.svg)条，至少插入![img](https://cdn.nlark.com/yuque/__latex/30ec88098f5570d8cb5dddda05629301.svg)条）

1. 参考SQL数据，由脚本自动生成：[📎验收数据.zip](https://www.yuque.com/attachments/yuque/0/2023/zip/29437275/1686492221764-b7ba2711-03b6-4a69-882e-de26d227ce9b.zip)
2. 批量执行时，所有sql执行完显示总的执行时间

1. 执行全表扫描`select * from account`，验证插入的数据是否正确（要求输出查询到![img](https://cdn.nlark.com/yuque/__latex/d3df89690bb9f1eb2c7e46882ad9383f.svg)条记录）

1. 考察点查询操作：

1. `select * from account where id = ?`
2. `select * from account where balance = ?`
3. `select * from account where name = "name56789"`，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/a67f549adc92ae7ed58082ebbbc38d50.svg)
4. `select * from account where id <> ?`
5. `select * from account where balance <> ?`
6. `select * from account where name <> ?`

1. 考察多条件查询与投影操作：

1. `select id, name from account where balance >= ? and balance < ?`
2. `select name, balance from account where balance > ? and id <= ?`
3. `select * from account where id < 12515000 and name > "name14500"`
4. `select * from account where id < 12500200 and name < "name00100"`，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/fc76f3e57849e54734eb2c56803d9401.svg)

1. 考察唯一约束：

1. `insert into account values(?, ?, ?)`，提示PRIMARY KEY约束冲突或UNIQUE约束冲突

1. 考察索引的创建删除操作、记录的删除操作以及索引的效果：

1. `create index idx01 on account(name)`
2. `select * from account where name = "name56789"`，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/e8227c1365280f1b3384459eaa53daea.svg)，要求![img](https://cdn.nlark.com/yuque/__latex/88a2f2d648d2bbbe372366c229580fe6.svg)
3. `select * from account where name = "name45678"`，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/72ac33d0d9858af251b540a40e4a071a.svg)
4. `select * from account where id < 12500200 and name < "name00100"`，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/c7f5ac7f2e1f9d307db65fab3b8bb664.svg)，比较![img](https://cdn.nlark.com/yuque/__latex/fc76f3e57849e54734eb2c56803d9401.svg)和![img](https://cdn.nlark.com/yuque/__latex/c7f5ac7f2e1f9d307db65fab3b8bb664.svg)
5. `delete from account where name = "name45678"`
6. `insert into account values(?, "name45678", ?)`
7. `drop index idx01`
8. 执行(c)的语句，此处记录执行时间![img](https://cdn.nlark.com/yuque/__latex/6aea8b94ebb6edd43f2e9cd6705a1838.svg)，要求![img](https://cdn.nlark.com/yuque/__latex/cc68fa880d9e06f3811b775bf6e4e6ce.svg)

1. 考察更新操作：

1. `update account set id = ?, balance = ? where name = "name56789";`

 并通过`select`操作验证记录被更新

1. 考察删除操作：

1. `delete from account where balance = ?`，并通过`select`操作验证记录被删除
2. `delete from account`，并通过`select`操作验证全表被删除
3. `drop table account`，并通过`show tables`验证该表被删除







drop index idx01 没有table name框架代码给出的接口就是有tablename 和indexname的, 是需要自己实现映射tablename的吗?







[![YingChengJun](https://cdn.nlark.com/yuque/0/2024/jpeg/25540491/1713423592889-avatar/7ddaca76-6f7e-4645-9b56-23a2439dd6fd.jpeg?x-oss-process=image%2Fresize%2Cm_fill%2Cw_64%2Ch_64%2Fformat%2Cpng)](https://www.yuque.com/yingchengjun)

[YingChengJun](https://www.yuque.com/yingchengjun)[2022-05-30 14:12](https://www.yuque.com/yingchengjun/minisql/vyufyi#comment-22326834)

可以不按照这里的标准，能够实现删除一个索引即可。