
skynet = /home/zgww/pe/skynet
f = 
cpdir = ../lua_spider/lib/

ifeq ($(shell uname), Darwin)

f += -undefined dynamic_lookup
skynet = /Users/zgww/ws/skynet
cpdir = ../lua_spider/lib/mac/

endif

i = -I$(skynet)/3rd/lua 
i += -I$(skynet)/skynet-src 

l = -lpthread

all : 
	gcc -fPIC -shared -o sncurl.so c-src/sncurl.c -lcurl $(i) $(l) $(f)
	cp -f sncurl.so $(cpdir)
easy : 
	gcc -o easy.out c-src/easy.c -lcurl -lpthread
	./easy.out


