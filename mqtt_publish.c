#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <mosquitto.h>
#include "mqtt_publish.h"

#define BUFFER_LEN 50	  // Messages stockés max
#define TIMEOUT 10		  // Secondes de connexion inactive avant déconnexion
#define MESSAGE_ALLOC 100 // Octets alloués pour chaque message

typedef struct CThread CThread;

void publishAll();
void mqtt_connexion();
void mqtt_launch(char* string);
char* find_space();

int createThread(CThread* thread);
void* send_thread(void *voidptr);
void* loop_thread(void *voidptr);

void on_connect(struct mosquitto *mosq, void *userdata, int result);
void on_disconnect(struct mosquitto *mosq, void *userdata, int result);


struct CThread {
    pthread_t thread_ID;
    void *thread_func;
	void *args;
};

char *host = "127.0.0.1";
int port = 1883;

char* topic = "avion";
char* messages[BUFFER_LEN];

bool connected = false;
struct mosquitto *client;

static unsigned int timevar = 0;
CThread loop_thr;

/*
//  GLOBAL FUNCTIONS
*/

void mqtt_send(char* str){
	CThread thr;
	thr.thread_ID = 0;
	thr.thread_func = send_thread;
	thr.args = str;
	createThread(&thr);
}

void mqtt_end(){
	for(int i = 0; i < BUFFER_LEN; i++){
		if(messages[i] == NULL) continue;
		free(messages[i]);
	}
	mosquitto_lib_cleanup();
}

/*
//  THREADS FUNCTIONS
*/

int createThread(CThread* thread){
	int retc = pthread_create(&thread->thread_ID, NULL, thread->thread_func, (void*)thread->args);
	int retd = pthread_detach(thread->thread_ID);
	if(retc != 0 || retd != 0){
		printf("Impossible de créer le Thread ...\n");
		return -1;
	}
    return 0;
}

void* loop_thread(void *voidptr){
	while(true){
		if(timevar > 0){
			publishAll();
			if(timevar == 1) {
				mosquitto_disconnect(client);
				break;
			}
			timevar--;
		}
		sleep(1);
	}
	return (void*)NULL;
}

void* send_thread(void *voidptr){
	char* strptr = (char*)voidptr;
	mqtt_launch(strptr);
	return (void*)NULL;
}

/*
//  CALLBACKS FUNCTIONS
*/

void on_connect(struct mosquitto *mosq, void *userdata, int result){
	if(!result){
		connected = true;
		loop_thr.thread_ID = 0;
		loop_thr.thread_func = loop_thread;
		createThread(&loop_thr);
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void on_disconnect(struct mosquitto *mosq, void *userdata, int result){
	connected = false;
}

/*
//  SYSTEM FUNCTIONS
*/


void publishAll(){
	for(int i = 0; i < BUFFER_LEN; i++){
		char* message = messages[i];
		if(message == NULL) continue;
		int msglen = strlen(message);
		if(msglen < 1) continue;
		mosquitto_publish(client, NULL, topic, msglen, message, 1, false);
		printf("'%s' envoyé !\n", message);
		memset(message, 0, msglen);
	}
}

char* find_space(){
	char* ret = NULL;
	for(int i = 0; i < BUFFER_LEN; i++){
		if(messages[i] == NULL){
			printf(">> Space %d found\n", i);
			messages[i] = malloc(sizeof(char)*MESSAGE_ALLOC);
			ret = messages[i];
			break;
		}
		if(strlen(messages[i]) < 1){
			ret = messages[i];
			break;
		}
	}
	return ret;
}

void mqtt_connexion(){
	client = mosquitto_new(NULL, true, NULL);
	if(!client){
		fprintf(stderr, "Error: Out of memory.\n");
		return;
	}
	mosquitto_connect_callback_set(client, on_connect);
	mosquitto_disconnect_callback_set(client, on_disconnect);

	int code = mosquitto_connect(client, host, port, 60);
	if(code != MOSQ_ERR_SUCCESS){
		connected = false;
		fprintf(stderr, "[%d]: Erreur lors de la connexion au serveur !.\n", errno);
		return;
	}
	mosquitto_loop_forever(client, -1,1);
}

void mqtt_launch(char* string){
	char* space = find_space();
	if(space == NULL){
		printf("No space found !! /!\\\n");
		return;
	}
	strcpy(space, string);
	timevar = TIMEOUT;
	if(!connected) {
		mqtt_connexion();
	}else{
		publishAll();
	} 
}