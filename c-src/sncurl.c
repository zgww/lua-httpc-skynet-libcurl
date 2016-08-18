#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <curl/multi.h>

#include <lua.h>
#include <lauxlib.h>

#include <pthread.h>

#define RESULT_DONE 0
#define RESULT_HEADER 1
#define RESULT_WRITE 2

#define MAX_WAIT_MSECS 30 * 1000 /* Wait max. 30 seconds */


struct req {
	struct curl_slist *list;
};

static CURLM *cm;
static pthread_t tid;


static void * __on_thread(void *);

static void __init(){
	static bool inited;
	if (inited) 
		return;

	inited = true;

	curl_global_init(CURL_GLOBAL_ALL);

	cm = curl_multi_init();

	pthread_create(&tid, NULL, __on_thread, NULL);
}

static void __info_read(){
	CURLMsg *msg;
	int msgs_left = 0;
	while (msg = curl_multi_info_read(cm, &msgs_left)){
		if (msg->msg != CURLMSG_DONE)
			continue;

        CURL *eh = msg->easy_handle;

		struct req *req;
		curl_easy_getinfo(eh, CURLINFO_PRIVATE, &req);
		__free_req(req);

		curl_multi_remove_handle(cm, eh);
		curl_easy_cleanup(eh);
	}
}
static void __free_req(struct req *req){
	free(req)
}


static void __perform_all(){
	int still_running = 0;
	int num_fds = 0;
	int i = 0; 


	curl_multi_perform(cm, &still_running);

	do {
		int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &num_fds);
		if (res != CURLM_OK) {
			fprintf(stderr, "curl_multi_wait failed.\n");
			exit(-1);
		}

		__info_read();
        curl_multi_perform(cm, &still_running);

	} while (still_running > 0);
}

static void * __on_thread(void *ud){ 
	while (1) {
		__perform_all();
		usleep(10 * 1000); // 10ms
	}
	printf("on curl_multi pthread end\n");
}
int main(){
	__init();

	CURL *eh = curl_easy_init();
	curl_easy_setopt(eh, CURLOPT_URL, "http://www.baidu.com/");

	curl_multi_add_handle(cm, eh);

	pthread_join(tid, NULL);

	return 0;
}

static const char * _lget_str_of_field(lua_State *ls, const char *key){
	lua_getfield(ls, -1, key);

	const char *v;
	if (lua_isnil(ls, -1)){ 
		v = NULL
	} else {
		v = lua_tostring(ls, -1);
	}
	lua_pop(ls, 1);
	return v;
}
/*
	headers = {
		'key: val', ...
	},
*/
static void __set_headers(lua_State *ls, CURL *eh, struct req *req){
	lua_getfield(ls, -1, "headers"); // headers tbl, tbl, ...

	if (lua_isnil(ls, -1))  
		goto __POP__;

	int len = lua_rawlen(ls, -1);
	int i;
	if (len == 0) 
		goto __POP__;


	for (i = 1; i <= len; i++) {
		lua_rawgeti(ls, -1, i); // header, headers tbl, tbl
		const char *header= lua_tostring(ls, -1);
		lua_pop(ls, 1); // headers tbl, tbl

		req->list = curl_slist_append(req->list, header);
	}

	curl_easy_setopt(eh, CURLOPT_HTTPHEADER, req->list);

__POP__ :
	lua_pop(ls, 1); // tbl, ...
}
/*
	cookie = 'name=xx;name=xx;..'
 */
static void __set_cookie(lua_State *ls, CURL *eh, struct req *req){
	lua_getfield(ls, -1, "cookie"); // cookie,  tbl, ...

	if (lua_isnil(ls, -1))
		goto __POP__;

	const char *cookie = lua_tostring(ls, -1);
	curl_easy_setopt(eh, CURLOPT_COOKIE, cookie);

__POP__ : 
	lua_pop(ls, 1);
}
/*
  param = {
  	method = 'GET|POST|PUT|DELTE|CUSTOMREQUEST',
	enctype = 'multipart/formdata|application/x-www-form-urlencoded',
	url = 'http://...|https://...',
	postfields = string, --such as : name=123&pwd=3434&...

	{key = str, type = 'params'},
	{key = str, filename = 'string', ['Content-Type'] = 'string', type = 'files'},

	params = {
	},
	files = {
	},
	headers = {
		'key', 'val', ...
	},
	cookie = 'name=xx;name=xx;...',
  }
*/
static int lrequest(lua_State *ls){
	CURL *eh = cury_easy_init();
	struct req *req = calloc(1, sizeof(struct req));
	curl_easy_setopt(eh, CURLOPT_PRIVATE, req);

	const char *method = _lget_str_of_field(ls, "method");
	const char *url = _lget_str_of_field(ls, "url");
	const char *enctype = _lget_str_of_field(ls, "enctype");
	const char *postfields = _lget_str_of_field(ls, "postfields");

	__set_headers(ls, eh, req);
	__set_cookie(ls, eh, req);

	return 0;
}


int luaopen_sncurl(lua_State *ls){
	luaL_Reg lib[] = {
		{"request", lrequest},
	};

	luaL_newlib(ls, lib);

	return 1;
}
