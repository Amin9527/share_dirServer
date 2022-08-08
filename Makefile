ALL: HttpServer upload_file
HttpServer: threadpool.hpp httpserver.cpp utils.hpp
	g++ -o $@ $^ -lpthread -std=c++11
upload_file: upload.cpp
	g++ -o $@ $^ -std=c++11
.PHONY: clean
clean:
	rm -f HttpServer upload_file
