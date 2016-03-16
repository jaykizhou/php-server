objs = rio.o server.o zhou.o
zhou:$(objs)
	gcc -o zhou $(objs) 
rio.o:rio.c rio.h
	gcc -c rio.c 
server.o:server.c server.h rio.h
	gcc -c server.c
zhou.o:zhou.c server.h
	gcc -c zhou.c
clean:
	rm zhou $(objs)
