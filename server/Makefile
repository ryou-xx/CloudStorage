test:main.cpp base64.cpp
	g++ -o $@ $^ -std=c++17 -lpthread  -ljsoncpp -lbundle -levent -lz
gdb_test:main.cpp base64.cpp
	g++ -g -o $@ $^ -std=c++17 -lpthread -ljsoncpp  -lbundle -levent -lz
.PHONY:clean
clean:
	rm -rf test gdb_test ./deep_storage ./low_storage ./logfile storage.data
