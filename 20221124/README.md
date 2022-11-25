# 세마포어 간단 개념
세마포어는 음수가 될 수 없는 자연수 변수를 이용하여 다중 쓰레드 또는 다중 프로세스 간 동기화를 도와주는 도구입니다. 아래 그림에서와 같이 세마포어는 `sem_t` 타입의 변수와 `sem_post()`, `sem_wait()` 로 구성된다고 볼 수 있습니다. `sem_t` 타입의 변수는 음수가 될 수 없기 때문에 0인 상황에서 `sem_wait()`를 호출한 프로세스 또는 쓰레드는 자신이 `sem_t` 타입의 변수에 1을 빼도 음수가 되지 않는 상황까지 (다른 프로세스에 의해 `sem_post()`가 호출되기 전까지) blocking 상태에 빠지게 됩니다. 
실제로 `sem_t` 타입의 변수는 음수가 될 수 없지만 아래 예제에서는 설명을 간략히 하기 위해 음수를 지원하고, 만약 `sem_t`가 음수인 상황을 유발하면 해당 프로세스는 blocking 상태에 빠진다는 가정하에 예제를 설명해보겠습니다.

![](https://minio.godopu.net/docs/uploads/250d08d8-0371-44bb-a000-fa65a5ade53e.png)

아래 그림은 세마포어의 개념을 설명하는 간단한 예제 2개를 보여줍니다. 

왼쪽 예제에서 프로그램 A, B, C는 sem이 0인 상황에서 `sem_wait()`를 호출합니다. 그림에서는 sem을 -3으로 표현했지만 실제로는 0인 상황이 유지되고, 프로그램 A, B, C가 `sem_wait()`를 완료하지 못하고 blocking 상태에 놓이게 됩니다. 이후 프로그램 D가 `sem_post()`를 한 번 호출하게 되면 sem이 1이되어 한 번의 `sem_wait()`를 허용하게 되고, 세 프로세스 중 한 프로세스가 blocking 상태에서 해제되어 `sem_wait()`를 완료하게 됩니다. 이 때 어떤 프로세스가 blocking에서 해제될지는 알 수 없습니다. 이렇게 3번의 `sem_post()`가 수행되면 모든 프로세스가 blocking에서 해제될 수 있습니다. 

오른쪽 예제에서는 프로그램 A가 먼저 `sem_post()`를 3번 호출하여 sem을 3으로 만들어놓았습니다. 때문에 이후 3번의 `sem_wait()` 호출에서는 프로세스 또는 쓰레드가 blocking 상태에 놓이지 않습니다. 

![](https://minio.godopu.net/docs/uploads/0c898174-4420-4b77-843e-177ae19e66be.png)


# 프로그램 개요
프로그램의 간략한 소개를 위해 음식 주문 예제를 통해 실습 프로그램을 간략히 설명해보겠습니다. 프로그램은 2개의 공유 메모리 (1234, 1235)와 각 공유 메모리의 접근을 제어하기 위한 2개의 세마포어 (1234_programAB_sem, 1235_programAB_sem)을 필요로 합니다. 해당 작업은 참고 코드 내 `makeShms()` 함수에 의해 수행됩니다. 

초기화 작업이 완료된 후 programA는 공유 메모리 1234에 주문 내용 길이 (4bytes)와 주문 내용을 작성하고, sem_post()를 호출하여 `1234_programAB_sem`에 대응되는 세마포어 변수의 값을 1 증가시킵니다. programB는 주문함에서 주문의 갯수를 먼저 확인한 후 주문이 있다면 (세마포어 변수 값이 0이 아니라면) 주문 내용을 읽고 음식을 준비 (공유 메모리로 부터 문자열을 읽은 후 이름을 뒤에 추가)합니다. 이후 배식구에 음식을 넣고 (공유 메모리 1235에 이름을 추가한 문자열을 쓰고), sem_post()를 통해 준비된 음식의 수를 1 증가시킵니다. 그럼 sem_wait()를 통해 음식을 대기하던 programA가 blocking 상태에서 해제되고, 준비된 음식을 가져갑니다.

![](https://minio.godopu.net/docs/uploads/2bac2d60-0385-468e-9ba6-3db9c8de73b3.jpg)


# 공유 메모리 초기화 
```c
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
```

# 메시지 쓰기
- s_shmaddr은 길이가 100 bytes인 배열의 첫 주소를 주정하고 있습니다.
- &s_shmaddr[4]는 배열의 5번째 원소의 주소를 나타냅니다.
- 문자열을 복사하거나 읽은 후에는 반드시 lenght 번째 원소에 0을 넣어주세요

## 공유메모리에 쓰는 함수
```c
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
```

## 공유메모리로 부터 읽는 함수
```c
int readFromShm(shmaddrs *shms, char *msg) {
  int length = -1;
  int *lengthField = shms->r_shmaddr;
  char *contentField = &shms->r_shmaddr[4];

  printf("---> wait %s\n", shms->r_sem_name);
  sem_wait(shms->r_sem); // 주문 음식을 기다림

  length = *lengthField;
  strncpy(msg, contentField, length);
  msg[length] = 0;

  printf("\tread message: %s\n", msg);

  return length;
}
```