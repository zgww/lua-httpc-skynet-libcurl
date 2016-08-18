

i = -I~/ws/skynet/3rd/lua
l = -L~/ws/skynet/3rd/lua -llua -lpthread

all : 
	gcc -fPIC -o sncurl c-src/sncurl.c -lcurl $(i) $(l)


easy : 
	gcc -o easy.out c-src/easy.c -lcurl -lpthread
	./easy.out


