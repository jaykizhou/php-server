zo:rio.c fastcgi.c server.c zhou.c
	gcc rio.c fastcgi.c server.c zhou.c -o zo 
m:
	gcc rio.c fastcgi.c server.c mylock.c zhou.c -lpthread -o zm 
