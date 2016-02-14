objects = server.o socket_server.o
out = ihome

ihome: $(objects)
	gcc -o  $(out) $(objects) -pthread -lsqlite3
	make clean
server.o : idebug.h
socket_server.o: idebug.h

.PHONY:clean
.PHONY:flush
clean:
	-rm $(objects)
flush:
	-rm $(out)
