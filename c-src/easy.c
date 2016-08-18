/* curl_multi_test.c
   Clemens Gruber, 2013
   <clemens.gruber@pqgruber.com>
   Code description:
    Requests 4 Web pages via the CURL multi interface
    and checks if the HTTP status code is 200.
   Update: Fixed! The check for !numfds was the problem.
*/

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <curl/multi.h>
#include <stdbool.h>
#include <pthread.h>

CURL *eh;


char header_buf[CURL_MAX_HTTP_HEADER + 1];

static size_t __on_header(char *buf, size_t size, size_t n, void *ud){
	size_t len = size * n;
	strncpy(header_buf, buf, len);
	header_buf[len] = '\0';
	printf("read head : %s\n", header_buf);
	return len;
}

static size_t __on_write(char *buf, size_t size, size_t n, void *ud){
	size_t len = size * n;
	strncpy(header_buf, buf, len);
	header_buf[len] = '\0';
	printf("read body  : %s\n", header_buf);
	return len;
}
static void * _th(void *ud){
	eh = curl_easy_init();
	curl_easy_setopt(eh, CURLOPT_URL, "http://www.baidu.com/");
	//curl_easy_setopt(eh, CURLOPT_VERBOSE, true);
	//curl_easy_setopt(eh, CURLOPT_HEADER, true);
	//curl_easy_setopt(eh, CURLOPT_HTTPPOST, true);
	curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, __on_header);
	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, __on_write);
}

static void __get(){
	//CURL *eh = curl_easy_init();
	//curl_easy_setopt(eh, CURLOPT_URL, "http://192.168.2.138:9999/api_doc/");
	//curl_easy_setopt(eh, CURLOPT_VERBOSE, true);
	//curl_easy_setopt(eh, CURLOPT_HEADER, true);
	////curl_easy_setopt(eh, CURLOPT_HTTPPOST, true);
	//curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, "GET");


	CURLcode succ = curl_easy_perform(eh);


	int status = 0;
	curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &status);
	printf("succ : %d %s %d\n", succ == CURLE_OK, curl_easy_strerror(succ), status);
}

int main(){
	curl_global_init(CURL_GLOBAL_ALL);


	pthread_t id;
	pthread_create(&id, NULL, _th, NULL);
	pthread_join(id, NULL);
	__get();

	curl_global_cleanup();
	return 0;
}
