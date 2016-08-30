
skynet = /home/zgww/pe/skynet

i = -I~/ws/skynet/3rd/lua 
i += -I$(skynet)/skynet-src 

l = -L~/ws/skynet/3rd/lua -llua -lpthread

all : 
	gcc -fPIC -shared -o sncurl.so c-src/sncurl.c -lcurl $(i) $(l)
	cp -f sncurl.so ../lua_spider/lib/sncurl.so


easy : 
	gcc -o easy.out c-src/easy.c -lcurl -lpthread
	./easy.out


