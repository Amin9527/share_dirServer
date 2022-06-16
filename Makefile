ALL: HttpServer upload
HttpServer: threadpool.hpp httpserver.cpp utils.hpp
	g++ -o $@ $^ -lpthread -std=c++11
upload: upload.cpp
	g++ -o $@ $^ -std=c++11
.PHONY: clean
clean:
	rm -f HttpServer upload
