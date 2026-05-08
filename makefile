CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  webserver.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server


polyglot-cpp: polyglot/cpp/server.cpp
	$(CXX) -std=c++17 -O2 -pthread -o polyglot/cpp/server $<

polyglot-rust: polyglot/rust/server.rs
	rustc -O -o polyglot/rust/server $<

polyglot-go: polyglot/go/server.go
	go build -o polyglot/go/server ./polyglot/go/server.go

polyglot-all: polyglot-cpp polyglot-rust polyglot-go

clean-polyglot:
	rm -f polyglot/cpp/server polyglot/rust/server polyglot/go/server
