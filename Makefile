objs = ./src/Test.o
down:$(objs)
	g++ $(objs) -std=c++17 -lpthread -I ./_include -o down
	mv down ./bin/

$(objs):

