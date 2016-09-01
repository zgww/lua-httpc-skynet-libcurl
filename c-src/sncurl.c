#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <curl/multi.h>

#include <lua.h>
#include <lauxlib.h>

#include <pthread.h>

#include <skynet.h>
#include <skynet_handle.h>
#include <skynet_server.h>
#include <assert.h>

#define RESULT_FAIL -1 
#define RESULT_DONE 0
#define RESULT_HEADER 1
#define RESULT_WRITE 2

#define MAX_WAIT_MSECS 30 * 1000 /* Wait max. 30 seconds */


struct req {
	struct curl_slist *list;
	struct curl_httppost *post;
	int64_t handle;
	int session;
};
struct msg {
	int result;
	int status;


	int buf_len;
	char buf[0];
};

static CURLM *cm;
static pthread_t tid;


static void * __on_thread(void *);

static void __notify(struct req *req, int result, int status, const char *buf, int buf_len){
	struct msg *msg = calloc(1, sizeof(struct msg) + buf_len);
	msg->result = result;
	msg->status = status;
	msg->buf_len = buf_len;

	if (buf)
		memcpy(msg->buf, buf, buf_len);

	struct skynet_context *ctx = skynet_handle_grab(req->handle);
	//printf("to %p:%d:%d. r : %d, status : %d\n", ctx, req->handle, req->session, result, status);

	skynet_context_send(ctx, msg, sizeof(struct msg), 0, PTYPE_RESPONSE, req->session);
	//printf("send done\n");
}

static size_t __on_header(char *buf, size_t size, size_t n, void *ud){
	//printf("__on_header\n");
	struct req *req = ud;
	size_t len = size * n;
	__notify(req, RESULT_HEADER, 0, buf, len);

	//strncpy(header_buf, buf, len);
	//header_buf[len] = '\0';
	//printf("read head : %s\n", header_buf);
	return len;
}

static size_t __on_write(char *buf, size_t size, size_t n, void *ud){
	//printf("__on_write\n");
	struct req *req = ud;
	size_t len = size * n;
	__notify(req, RESULT_WRITE, 0, buf, len);
	/**/
	//strncpy(header_buf, buf, len);
	//header_buf[len] = '\0';
	//printf("read body  : %s\n", header_buf);
	/**/
	return len;
}


static void __init(){
	static bool inited;
	if (inited) 
		return;

	inited = true;

	curl_global_init(CURL_GLOBAL_ALL);

	cm = curl_multi_init();

	pthread_create(&tid, NULL, __on_thread, NULL);
}

static void __free_req(struct req *req){
	curl_slist_free_all(req->list);
	curl_formfree(req->post);

	free(req);
}

static void __info_read(){
	CURLMsg *msg;
	int msgs_left = 0;
	while ((msg = curl_multi_info_read(cm, &msgs_left)) != 0){
		if (msg->msg != CURLMSG_DONE)
			continue;


		CURLcode result = msg->data.result;
        CURL *eh = msg->easy_handle;

		struct req *req = NULL;
		long status = -1;
		curl_easy_getinfo(eh, CURLINFO_PRIVATE, &req);
		curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &status);


		const char *buf = result == CURLE_OK ? NULL : curl_easy_strerror(result);
		int len = result == CURLE_OK ? 0 : strlen(buf);
		__notify(req, result == CURLE_OK ? RESULT_DONE : RESULT_FAIL, status, buf, len);

		//printf("free req\n");
		//printf("req is : %p eh %p ----\n", req, eh);
		__free_req(req);
		//printf("remove handle\n");

		curl_multi_remove_handle(cm, eh);
		//printf("cleanup\n");
		curl_easy_cleanup(eh);
		//printf("done\n");
	}
}


static void __perform_all(){
	int still_running = 0;
	int num_fds = 0;
	int i = 0; 


	curl_multi_perform(cm, &still_running);

	do {
		CURLcode res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &num_fds);
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
	//printf("on curl_multi pthread end\n");
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
	//printf("\t_lget_str_of_field key : %s\n", key);
	lua_getfield(ls, -1, key);
	//printf("\t_lget_str_of_field top : %d\n", lua_gettop(ls));

	const char *v;
	if (lua_isnil(ls, -1)){ 
		//printf("\t_lget_str_of_field is nil\n");
		v = NULL;
	} else {
		//printf("\t_lget_str_of_field is str\n");
		v = lua_tostring(ls, -1);
		//printf("\t_lget_str_of_field is str %s\n", v);
	}
	//printf("\t_lget_str_of_field pop\n");
	lua_pop(ls, 1);
	//printf("\t_lget_str_of_field pop succ\n");
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

static bool __has_field(lua_State *ls, const char *key){
	lua_getfield(ls, -1, key);
	bool has = !lua_isnil(ls, -1);
	lua_pop(ls, 1);
	return has;
}
static bool __has_type(lua_State *ls, const char *key){
	int len = lua_rawlen(ls, -1);
	int i;
	for (i = 1; i <= len; i++) {
		lua_rawgeti(ls, -1, i); // pair, ls
		lua_getfield(ls, -1, "type"); // type, pair, ls
		const char *t = lua_tostring(ls, -1);
		lua_pop(ls, 2);

		if (strcmp(t, key) == 0) {
			return true;
		}
	}
	return false;
}
//{key = str, val = str, type = 'param'},
//{key = str, val = str, filename = 'string', ['Content-Type'] = 'string', type = 'file'},
static void __set_multi(lua_State *ls, CURL *eh, struct req *req){
	int len = lua_rawlen(ls, -1);
	int i;

	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;
	for (i = 1; i <= len; i++) {
		lua_rawgeti(ls, -1, i); // pair, ls
		const char *type = _lget_str_of_field(ls, "type");
		const char *key = _lget_str_of_field(ls, "key");
		const char *val = _lget_str_of_field(ls, "val");
		const char *filename = _lget_str_of_field(ls, "filename");
		const char *content_type = _lget_str_of_field(ls, "Content-Type");

		if (strcmp(type, "file")) {

			if (filename && content_type) {
				curl_formadd(&post, &last,
					 CURLFORM_COPYNAME, key, CURLFORM_FILE, val,
					 CURLFORM_FILENAME, filename, CURLFORM_CONTENTTYPE, content_type, CURLFORM_END);

			} else if (filename) {
				curl_formadd(&post, &last,
					 CURLFORM_COPYNAME, key, CURLFORM_FILE, val,
					 CURLFORM_FILENAME, filename, CURLFORM_END);
			} else if (content_type) {
				curl_formadd(&post, &last,
					 CURLFORM_COPYNAME, key, CURLFORM_FILE, val,
					 CURLFORM_CONTENTTYPE, content_type, CURLFORM_END);
			} else {
				curl_formadd(&post, &last, CURLFORM_COPYNAME, key, CURLFORM_FILE, val, CURLFORM_END);
			}

		} else { // key = str, val = str
			curl_formadd(&post, &last, CURLFORM_COPYNAME, key, CURLFORM_COPYCONTENTS, val, CURLFORM_END);
		}
		lua_pop(ls, 1); // ls;
	}

	req->post = post;
	curl_easy_setopt(eh, CURLOPT_HTTPPOST, post);
}
/*
  param = {
	url = 'http://...|https://...',
  	method = 'GET|POST|PUT|DELTE|HEAD|CUSTOMREQUEST',
	postfields = string, --such as : name=123&pwd=3434&...

	{key = str, val = str, type = 'param'},
	{key = str, val = str, filename = 'string', ['Content-Type'] = 'string', type = 'file'},

	headers = {
		'key:val ', '', ...
	},
	cookie = 'name=xx;name=xx;...',
	handle = int,
	session = int,
	accept_encoding = str, 
	proxy = '[scheme(default HTTP proxy)://]host[:port(default 1080)]'
} */
static int lrequest(lua_State *ls){
	assert(lua_gettop(ls) == 1 && "sncurl.request argc ~= 1");
	__init();

	CURL *eh = curl_easy_init();

	struct req *req = calloc(1, sizeof(struct req));
	//printf("new req %p eh : %p\n", req, eh);

	lua_getfield(ls, -1, "handle"); // h, ls
	req->handle = (int64_t)lua_tointeger(ls, -1);

	lua_getfield(ls, -2, "session"); // s, h, ls
	req->session = (int64_t)lua_tointeger(ls, -1);
	lua_pop(ls, 2); // ls

	//printf("get hs : %d %d top : %d\n", (int)req->handle, req->session, lua_gettop(ls));

	curl_easy_setopt(eh, CURLOPT_PRIVATE, req);

	//printf("get method %s\n", "nil");
	const char *accept_encoding = _lget_str_of_field(ls, "accept_encoding");
	//printf("get method %s\n", "nil");
	const char *method = _lget_str_of_field(ls, "method");
	//printf("get method %s\n", method == NULL ? "nil" : method);
	const char *url = _lget_str_of_field(ls, "url");
	//printf("get url %s\n", url == NULL ? "nil" : url);
	const char *postfields = _lget_str_of_field(ls, "postfields");
	//printf("get postfields %s\n", postfields ?  postfields : "nil");
	const char *proxy = _lget_str_of_field(ls, "postfields");
	//printf("get proxy %s\n", proxy ?  proxy : "nil");

	assert(url && "must set url");

	if (!method) method = postfields ? "POST" : "GET";

	//printf("get arg : %s %s %s\n", method, url, postfields ? postfields : "nil");

	bool has_file = __has_type(ls, "file");
	//printf("has file %s\n", has_file ? "Y" : "N");

	__set_headers(ls, eh, req);
	__set_cookie(ls, eh, req);

	if (has_file) {
		//printf("set multi  :%s\n", method);
		__set_multi(ls, eh, req);
	} else if (strcmp(method, "GET") == 0) {
		//printf("set CURLOPT_GET  :%s\n", method);
		curl_easy_setopt(eh, CURLOPT_HTTPGET, 1);
	} else if (strcmp(method, "POST")) {
		//printf("set CURLOPT_POST  :%s\n", method);
		curl_easy_setopt(eh, CURLOPT_POST, 1);
	} else  {
		//printf("set customrequest  :%s\n", method);
		curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, method);
	}

	if (!has_file && postfields)  {
		//printf("set postfields :%s\n", postfields);
		curl_easy_setopt(eh, CURLOPT_POSTFIELDS, postfields);
	}

	//printf("set url : %s\n", url);
	curl_easy_setopt(eh, CURLOPT_URL, url);
	//printf("set write function\n");
	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, __on_write);
	//printf("set write data\n");
	curl_easy_setopt(eh, CURLOPT_WRITEDATA, req);

	//printf("set header func\n");
	curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, __on_header);
	//printf("set header data\n");
	curl_easy_setopt(eh, CURLOPT_HEADERDATA, req);
	//printf("set proxy \n");
	if (proxy) 
		curl_easy_setopt(eh, CURLOPT_PROXY, proxy);

	if (accept_encoding) 
		curl_easy_setopt(eh, CURLOPT_ACCEPT_ENCODING, accept_encoding);

	//printf("curl multi add handle\n");
	curl_multi_add_handle(cm, eh);

	return 0;
}
static int lget_result(lua_State *ls){
	struct msg *msg = lua_touserdata(ls, -1);
	lua_pushinteger(ls, msg->result);
	return 1;
}
static int lget_status(lua_State *ls){
	struct msg *msg = lua_touserdata(ls, -1);
	lua_pushinteger(ls, msg->status);
	return 1;
}
static int lget_buf(lua_State *ls){
	struct msg *msg = lua_touserdata(ls, -1);
	if (msg->buf_len > 0)
		lua_pushlstring(ls, msg->buf, msg->buf_len);
	else
		lua_pushnil(ls);
	return 1;
}
static int lescape(lua_State *ls){
	const char *str = lua_tostring(ls, -1);
	char *out = curl_easy_escape(NULL, str, 0);
	lua_pushstring(ls, out);
	curl_free(out);
	return 1;
}
static int lunescape(lua_State *ls){
	const char *str = lua_tostring(ls, -1);
	char *out = curl_easy_unescape(NULL, str, 0, 0);
	lua_pushstring(ls, out);
	curl_free(out);
	return 1;
}

int luaopen_sncurl(lua_State *ls){
	luaL_Reg lib[] = {
		{"request", lrequest},
		{"get_result", lget_result},
		{"get_status", lget_status},
		{"get_buf", lget_buf},
		{"escape", lescape},
		{"unescape", lunescape},
		{NULL, NULL},
	};

	luaL_newlib(ls, lib);

	return 1;
}
