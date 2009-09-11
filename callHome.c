/* callHome.c */

#include "platform.h"
#include <microhttpd.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

struct cometData {
	int sendCount;
	struct cometData *next_cd;
	long long ch_id;
	pthread_mutex_t ch_mutex;
	pthread_cond_t  ch_cond;
	char *httpDomain;
	long long extraData;
} *cdList = NULL;

pthread_mutex_t cdlMutex;

char * callHomeScript = "<script type=\"text/javascript\">top.postMessage(\"CallHome %lld\",\"%s\")</script>";
/*char * callHomeScript = "<script type=\"text/javascript\">alert(\"CallHome %lld : %s\")</script>";*/

static int toClient(void *cls, uint64_t pos, char *buf, int max) {
    struct timespec abstime = { 0, 0 };
    struct timeval curtime;
	struct cometData *cd = cls;
	int slb, rslt;
	char * keepAliveMsg = "<script type=\"text/javascript\">top.postMessage(\"CallHome keepalive\",\"%s\")</script>";
	char * startingMsg = "<script type=\"text/javascript\">top.postMessage(\"CallHome starting %lld\",\"%s\")</script>";
	char * restartMsg = "<script type=\"text/javascript\">location.reload(1)</script>";

	if( cd->sendCount++ == 0) {
		if (max < strlen(startingMsg)+10) return 0;
		sprintf(buf,startingMsg,cd->ch_id,cd->httpDomain);
	} else if (cd->sendCount > 1000) {
		if (max < strlen(restartMsg)+10,cd->httpDomain) return 0;
		sprintf(buf,restartMsg);
	} else {
		pthread_mutex_lock(&cd->ch_mutex);
		gettimeofday(&curtime, NULL);
		abstime.tv_sec = curtime.tv_sec + 30;

		rslt = pthread_cond_timedwait(&cd->ch_cond, &cd->ch_mutex, &abstime);
		pthread_mutex_unlock(&cd->ch_mutex);
		if( rslt == ETIMEDOUT) {
			/* send a keepalive */
			if( max < strlen(keepAliveMsg)+1) return 0;
			sprintf(buf,keepAliveMsg,cd->httpDomain);
		} else { /* if rslt==0 */
			/* at this point we want to send some data */
			if (max < strlen(callHomeScript)+strlen(cd->httpDomain)+10) return 0;
			sprintf(buf,callHomeScript,cd->extraData,cd->httpDomain);
		}
	}
	slb = strlen(buf);
	memset (&buf[slb], ' ', max-slb-1);
	buf[max-1] = '\n';
	return max;
}

static int callHomeIn(void *cls, struct MHD_Connection *connection,
		const char *url, const char *method, const char *version,
		const char *upload_data, size_t *upload_data_size, void **ptr) {
	static int aptr;
	struct MHD_Response *response;
	int ret;
	long long id;
	long long extra;
	char *httpDomain;

	if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
		return MHD_NO; /* unexpected method */
	if (&aptr != *ptr) {
		/* do never respond on first call */
		*ptr = &aptr;
		return MHD_YES;
	}
	*ptr = NULL; /* reset when done */
	if (strcmp(&url[1],"idcreate")==0) {
		/* expect parameters id and extra */
		struct cometData *cd;
		id = atoll(MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "id"));
		pthread_mutex_lock(&cdlMutex);
		for(cd = cdList; cd!=NULL; cd=cd->next_cd) {
			if( id==cd->ch_id) {
				break;
			}
		}
		if( cd==NULL) {
			cd = (struct cometData *)malloc(sizeof(struct cometData));
			cd->next_cd = cdList;
			cd->ch_id = id;
			pthread_mutex_init(&cd->ch_mutex, NULL);
			pthread_cond_init (&cd->ch_cond, NULL);
			cdList = cd;
			response = MHD_create_response_from_data(strlen("OK"),
								(void *) "OK", MHD_NO, MHD_NO);
			ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		} else {
			response = MHD_create_response_from_data(strlen("Failed"),
								(void *) "Failed", MHD_NO, MHD_NO);
			ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
		}
		pthread_mutex_unlock(&cdlMutex);
	} else if (strcmp(&url[1],"fromserver")==0) {
		/* expect parameters id and extra */
		struct cometData *cd;
		id = atoll(MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "id"));
		extra = atoll(MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "extra"));
		for(cd = cdList; cd!=NULL; cd=cd->next_cd) {
			if( id==cd->ch_id) {
				pthread_mutex_lock(&cd->ch_mutex);
				cd->extraData = extra;
				pthread_cond_signal(&cd->ch_cond);
				pthread_mutex_unlock(&cd->ch_mutex);
				break;
			}
		}
		if( cd==NULL) {
			/* maybe should do something in case client has disappeared temporarily?? */
		}
		response = MHD_create_response_from_data(strlen("OK"),
					(void *) "OK", MHD_NO, MHD_NO);
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	} else if (strcmp(&url[1],"toclient")==0) {
		/* expect parameters id and httpDomain */
		struct cometData *cd;
		id = atoll(MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "id"));
		httpDomain = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "httpDomain");
		for(cd = cdList; cd!=NULL; cd=cd->next_cd) {
			if( id==cd->ch_id) {
				break;
			}
		}
		if( cd==NULL) {
			response = MHD_create_response_from_data(strlen("not found"),
						(void *) "not found", MHD_NO, MHD_NO);
			ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
		} else {
			if (cd->httpDomain!=NULL && strcmp(cd->httpDomain,httpDomain)!=0) { /* unlikely */
				free(cd->httpDomain);
				cd->httpDomain = NULL;
			}
			if( cd->httpDomain == NULL) {
				cd->httpDomain = (char *)malloc(strlen(httpDomain)+1);
				strcpy(cd->httpDomain,httpDomain);
			}
			cd->sendCount = 0;
			cd->extraData = 0;
			response = MHD_create_response_from_callback (-1, 256, &toClient, (void *)cd, NULL);
			ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		}
	} else { /* not a known destination */
		response = MHD_create_response_from_data(strlen("Eh?"),
					(void *) "Eh?", MHD_NO, MHD_NO);
		ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
	}
	MHD_destroy_response(response);
	return ret;
}

int main(int argc, char * const *argv) {
	struct MHD_Daemon *d;

	if (argc != 2) {
		printf("usage: %s PORT\n", argv[0]);
		return 1;
	}
	pthread_mutex_init(&cdlMutex,NULL);
	d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG, atoi(
			argv[1]), NULL, NULL, &callHomeIn, NULL, MHD_OPTION_END);
	if (d == NULL)
		return 1;
	for(;;) sleep(600);
	/* yikes */
	MHD_stop_daemon(d);
	return 1;
}
