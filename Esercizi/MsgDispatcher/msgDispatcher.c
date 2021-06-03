#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CHILDREN    5
#define BUFFER_DIM      256
#define PIPE_READ       0
#define PIPE_WRITE      1

pthread_t killerThread;
pthread_mutex_t mutex;
int pids[MAX_CHILDREN] = {0}; // Se pari a 0 è libero, altrimenti è occupato
char lastString[BUFFER_DIM] = {0};
int parentToChildPipes[MAX_CHILDREN][2];
int shouldCycle = 1;

int freeSpotForProcess(int *pidList, int len) {
    int index = 0;
    for(int i = 0; i < len; i++) {
        if (pidList[i] == 0) {
            return i;
        }
    }
    
    return -1;
}

void* killerRoutine(void *args) {
    int signo = *(int*)args;

    FILE* logFile = fopen("/tmp/log.txt", "a+");
    if(! logFile) {
        printf("[THREAD] Unable to log\n");
        return (void*)0;
    }

    fprintf(logFile, "[THREAD] I'm a new thread generated by signal %d\n", signo);
    fprintf(logFile, "[THREAD] Sending message to children\n");

    for(int i = 0; i < MAX_CHILDREN; i++) {
        if (pids[i] == 0) {
            continue;
        }

        printf("[THREAD] Msg '%s' to child %d\n", lastString, pids[i]);
        
        write(parentToChildPipes[i][PIPE_WRITE], lastString, strlen(lastString));

        pids[i] = 0;
    }

    fprintf(logFile, "[THREAD] Children reset\n");
    fclose(logFile);

    return (void*)0;
}

void sigUsrHandler(int signo) {
    if (signo != SIGUSR1 && signo != SIGUSR2) {
        return;
    }

    pthread_create(&killerThread, NULL, killerRoutine, (void*)&signo);
    pthread_join(killerThread, NULL);
}

void terminateHandler(int signo) {
    if (signo != SIGKILL) {
        return;
    }

    shouldCycle = 0;
}

int main() {
    char buffer[BUFFER_DIM] = {0};
    int isNumber = 1;   

    // Inizializzo il mutex
    pthread_mutex_init(&mutex, NULL);

    // Imposto i segnali
    signal(SIGUSR1, sigUsrHandler);
    signal(SIGUSR2, sigUsrHandler);
    signal(SIGKILL, terminateHandler);

    // Stampo il pid
    printf("[MAIN] Main pid is %d\n", getpid());

    while(shouldCycle) {
        // Setup
        memset(buffer, 0, BUFFER_DIM);
        isNumber = 1;

        // Ricava l'input da stdin e controllo se è un numero 
        char c;
        int index = 0;
        while((c = getchar()) != '\n' && index < BUFFER_DIM) {
            buffer[index] = c;
            index++;

            if(!isdigit(c)) {
                isNumber = 0;
            }
        }

        // Se è un numero e posso, creo un figlio
        if (isNumber) {
            int index = freeSpotForProcess(pids, MAX_CHILDREN);

            if (index == -1) {
                printf("Numero massimo di processi raggiunto.\n");
                continue;
            }

            // Creo le pipe
            pipe(parentToChildPipes[index]);

            // Creo il figlio
            int pid = fork();

            // Figlio
            if(! pid) {
                // Chiudo la pipe in lettura
                close(parentToChildPipes[index][PIPE_WRITE]);

                printf("[CHLD] I'm a new child %d waiting for a message from my father.\n", getpid());

                // Resta in attesa di un messaggio dal padre
                memset(buffer, 0, BUFFER_DIM);
                int bytes = read(parentToChildPipes[index][PIPE_READ], buffer, BUFFER_DIM);

                printf("[CHLD] I received the following message: '%s'\n", buffer);
                close(parentToChildPipes[index][PIPE_READ]);

                return 0;
            }
            // Padre
            else {
                close(parentToChildPipes[index][PIPE_READ]);
                pids[index] = pid;
            }
        }

        // Altrimenti è una stringa, quindi la salvo
        else {
            memset(lastString, 0, BUFFER_DIM);
            strncpy(lastString, buffer, strlen(buffer));

            printf("[MAIN] Msg '%s' saved\n", lastString);
        }
    }

    while(wait(NULL) > 0);

    for(int i = 0; i < MAX_CHILDREN; i++) {
        close(parentToChildPipes[i][PIPE_READ]);
    }
    close(parentToChildPipes)

    return 0;
}