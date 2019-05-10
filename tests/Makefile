SHELL = /bin/sh
CC = g++

CFLAG = -W -Wall -g -Wno-reorder -Wno-unused-parameter -Wno-format -Wno-sign-compare -Wno-implicit-fallthrough -DDEBUG -DHAVE_EPOLL

INCLUDE_PATH = -I. -I../ -I../event/
LIBS_PATH = ../event/libnc_event.a
LUA_INCLUDE_PATH = -I../deps/LuaJIT-2.1/src/
LUA_LIBS_PATH = ../deps/LuaJIT-2.1/src/libluajit.a -ldl
YAML_INCLUDE_PATH = -I../deps/yaml-0.1.4/include
YAML_LIBS_PATH = ../deps/yaml-0.1.4/src/.libs/libyaml.a

rbtree:
	$(CC) $(CFLAG) $(INCLUDE_PATH) nc_rbtree_test.cpp \
	-o rbtree $(LIBS_PATH)

string:
	$(CC) $(CFLAG) $(INCLUDE_PATH) nc_string_test.cpp \
	-o string $(LIBS_PATH)

log:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	../nc_log.cpp ../nc_util.cpp nc_log_test.cpp \
	-o log $(LIBS_PATH)

util:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	../nc_log.cpp ../nc_util.cpp nc_util_test.cpp \
	-o util $(LIBS_PATH)

mbuf:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	nc_mbuf_test.cpp \
	-o mbuf $(LIBS_PATH)

test:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	../nc_test.cpp nc_test_test.cpp \
	-o test $(LIBS_PATH)

main:
	$(CC) $(CFLAG) $(INCLUDE_PATH) $(YAML_INCLUDE_PATH) \
	../nc_log.cpp ../nc_util.cpp ../nc_proxy.cpp ../nc_core.cpp ../nc_connection.cpp \
	../nc_client.cpp ../nc_conf.cpp ../nc_server.cpp ../nc_message.cpp ../nc_hashkit.cpp \
	../nc.cpp -o main  $(LIBS_PATH) $(YAML_LIBS_PATH)

queue:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	../nc_log.cpp ../nc_util.cpp nc_queue_test.cpp \
	-o queue $(LIBS_PATH)

proxy:
	$(CC) $(CFLAG) $(INCLUDE_PATH) \
	../nc_log.cpp ../nc_util.cpp ../nc_proxy.cpp ../nc_core.cpp ../nc_connection.cpp \
	../nc_client.cpp \
	nc_proxy_test.cpp \
	-o proxy $(LIBS_PATH)

lua:
	$(CC) $(CFLAG) $(INCLUDE_PATH) $(LUA_INCLUDE_PATH) \
	nc_lua_test.cpp \
	-o lua $(LIBS_PATH) $(LUA_LIBS_PATH)

conf:
	$(CC) $(CFLAG) $(INCLUDE_PATH) $(YAML_INCLUDE_PATH) \
	../nc_conf.cpp ../nc_log.cpp ../nc_util.cpp \
	nc_conf_test.cpp \
	-o conf $(LIBS_PATH) $(YAML_LIBS_PATH)

hashkit:
	$(CC) $(CFLAG) $(INCLUDE_PATH) $(YAML_INCLUDE_PATH) \
	../nc_conf.cpp ../nc_log.cpp ../nc_util.cpp ../nc_hashkit.cpp \
	nc_hashkit_test.cpp \
	-o hashkit $(LIBS_PATH) $(YAML_LIBS_PATH)

clean:
	rm -f *.o rbtree string log util mbuf test main queue proxy lua conf hashkit