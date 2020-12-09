# hp-server
C++ Linux Server
- [x] 单线程I/O多路复用 epoll eventloop (最初版本没有工作线程)
  - 设置套接字为非阻塞模式，否则recv, send必须要求读完所有字节才返回
  - 处理accept连接时支持 LT / ET 模式
  - 处理read,write时支持 LT / ET 模式
- [x] 定时器管理连接：对每个连接设置定时器，读写会刷新调整定时器，超时会关闭连接
  - 通过排序双向链表实现
  - 通过红黑树/大顶堆实现
  - (多线程epoll) 通过无锁数据结构实现, e.g.RCU
- [x] 信号处理
  - 连接关闭时SIGPIPE信号处理
  - SIGTERM信号处理
  - 定时器发送的SIGALRM信号处理
- [x] 半同步半反应堆模式(Half-Sync/Half-Reactor, HSHR): 三层结构
  - 工作队列通过stl链表实现
  - 主线程(reactor)往工作队列中插入任务，工作线程通过竞争来取得任务并执行它
  - 支持reactor/proactor的read, write，对于proactor模式：通过同步I/O模拟
  - 线程池
- [ ] 处理http请求，并作出正确响应 
  - http长连接，tcp长连接
  - 对http服务器，通过webbench压测
- [ ] 输出服务器日志LOG
- [ ] 引入GoogleTest单元测试                   
- [ ] I/O多路复用 多主线程epoll
- [ ] 支持mysql服务
  - mysql连接池
  - 并发写压测
- [ ] 支持redis服务
  - 更大并发量，并发读压测
- [ ] 更多tcp连接或者http更高版本，https的支持
- [ ] mysql, redis读写分离
- [ ] 分布式：5台机器支持，应对100000并发
