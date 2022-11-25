#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 100

typedef struct shmaddrs {
  void *s_shmaddr;
  void *r_shmaddr;
  char s_sem_name[20];
  char r_sem_name[20];
  sem_t *s_sem;
  sem_t *r_sem;

} shmaddrs;

shmaddrs *makeShms(int s_key, int r_key, char *sem_name);
void writeToShm(shmaddrs *shms, char *msg, int length);
int readFromShm(shmaddrs *shms, char *msg);
int readLine(char *line, int size);

int main() {
  char sendBuf[BUF_SIZE], recvBuf[BUF_SIZE];
  int retValue;
  shmaddrs *shms;

  shms = makeShms(1235, 1234, "programAB_sem");
  if (shms == NULL) {
    perror("create shared memory error");
  }

  while (1) {
    // receive the message from peer (programA)
    retValue = readFromShm(shms, recvBuf);
    if (retValue < 0) {
      perror("read message from shared memory error");
    }

    // concat ' postfix' to received string
    strncpy(sendBuf, recvBuf, retValue);
    sendBuf[retValue] = 0;
    strncat(sendBuf, " postfix", BUF_SIZE);

    // send the line to peer (programA)
    writeToShm(shms, sendBuf, strlen(sendBuf));

    // if 'end' is received from peer (programA), then break
    if (strncmp(recvBuf, "end", 3) == 0) {
      break;
    }

    puts("");
  }

  // free ids memory
  free(shms);
}

shmaddrs *makeShms(int s_key, int r_key, char *sem_name) {
  int s_shmid, r_shmid;
  shmaddrs *shms = (shmaddrs *)malloc(sizeof(shmaddrs));

  s_shmid = shmget(s_key, BUF_SIZE, IPC_CREAT | 0666);
  if (s_shmid == -1) {
    return NULL;
  }
  r_shmid = shmget(r_key, BUF_SIZE, IPC_CREAT | 0666);
  if (r_shmid == -1) {
    return NULL;
  }

  shms->s_shmaddr = shmat(s_shmid, NULL, 0);
  if (shms->s_shmaddr == (void *)-1) {
    return NULL;
  }
  shms->r_shmaddr = shmat(r_shmid, NULL, 0);
  if (shms->r_shmaddr == (void *)-1) {
    return NULL;
  }

  sprintf(shms->s_sem_name, "%d_%s", s_key, sem_name);
  sprintf(shms->r_sem_name, "%d_%s", r_key, sem_name);
  shms->s_sem = sem_open(shms->s_sem_name, O_CREAT, 0666, 0);
  if (shms->s_sem == SEM_FAILED) {
    return NULL;
  }
  shms->r_sem = sem_open(shms->r_sem_name, O_CREAT, 0666, 0);
  if (shms->r_sem == SEM_FAILED) {
    return NULL;
  }

  printf("shmaddrs for (write, send): %p, %p\n", shms->s_shmaddr,
         shms->r_shmaddr);
  return shms;
}

void writeToShm(shmaddrs *shms, char *msg, int length) {
  int *lengthField = shms->s_shmaddr;
  char *contentField = &shms->s_shmaddr[4];

  *lengthField = length;

  strncpy(contentField, msg, length);
  contentField[length] = 0;

  sem_post(shms->s_sem); // 음식 전달
  printf("---> post %s\n", shms->s_sem_name);
  printf("\twrite message: %s\n", msg);
}

int readFromShm(shmaddrs *shms, char *msg) {
  int length = -1;
  int *lengthField = shms->r_shmaddr;
  char *contentField = &shms->r_shmaddr[4];

  printf("---> wait %s\n", shms->r_sem_name);
  sem_wait(shms->r_sem); // 주문을 기다림

  length = *lengthField;
  strncpy(msg, contentField, length);
  msg[length] = 0;

  printf("\tread message: %s\n", msg);

  return length;
}

int readLine(char *line, int size) {
  char *retValue = NULL;
  retValue = fgets(line, size, stdin);
  retValue[strlen(retValue) - 1] = 0;

  return retValue != NULL;
}
