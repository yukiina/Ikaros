objs = Test.o
src = Test.cpp
test:$(objs) 
	g++ $(objs) -o test -lpthread -lstdc++fs -std=c++17 -I ../_include

$(objs):$(src)
	g++ -c $^ -o $@ -I ../_include -lstdc++fs -std=c++17

.PHONY:clean
clean:
	-rm -rf $(objs)
	-rm -rf test
