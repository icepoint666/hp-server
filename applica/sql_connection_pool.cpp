#include "sql_connection_pool.h"

connection_pool::connection_pool(){
	cur_cunn = 0;
	free_conn = 0;
}

connection_pool *connection_pool::GetInstance(){
	static connection_pool conn_pool;
	return &conn_pool;
}

//构造初始化 
void connection_pool::init(std::string url_, std::string user_, std::string password_, std::string db_name_, int port_, int max_conn_){
	url = url_;
	port = port_;
	user = user_;
	password = password_;
	db_name = db_name_;

	for (int i = 0; i < max_conn; i++){
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL){
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(), db_name.c_str(), port, NULL, 0);
		if (con == NULL){
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		conn_list.push_back(con);
		++free_conn;
	}

	reserve = sem(free_conn);
    max_conn = free_conn;
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::get_connection(){
	MYSQL *con = NULL;

	if (0 == conn_list.size())
		return NULL;

	reserve.wait();
	
	lk.lock();
	con = conn_list.front();
	conn_list.pop_front();
	--free_conn;
	++cur_conn;
	lk.unlock();
	return con;
}

//释放当前使用的连接
bool connection_pool::release_connection(MYSQL *con){
	if (NULL == con)
		return false;

	lk.lock();
	conn_list.push_back(con);
	++free_conn;
	--cur_conn;
	lk.unlock();

	reserve.post();
	return true;
}

//销毁数据库连接池
void connection_pool::destory_pool(){
	lk.lock();
	if (conn_list.size() > 0){
		std::list<MYSQL *>::iterator it;
		for (it = conn_list.begin(); it != conn_list.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		cur_conn = 0;
		free_conn = 0;
		conn_list.clear();
	}
	lk.unlock();
}

//当前空闲的连接数
int connection_pool::get_free_conn(){
	return this->free_conn;
}

connection_pool::~connection_pool(){
	destory_pool();
}

connection_RAII::connection_RAII(MYSQL **SQL, connection_pool *conn_pool){
	*SQL = conn_pool->get_connection();
	connRAII = *SQL;
	poolRAII = conn_pool;
}

connection_RAII::~connection_RAII(){
	poolRAII->release_connection(connRAII);
}
