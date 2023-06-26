/*
 * Copyright(c) 2021-2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * 2023.05.06 컴퓨터학부 2019033936 이승섭 - 코드 분석 및 스핀락 개념 공부 및 이해
 * 2023.05.07 컴퓨터학부 2019033936 이승섭 - compare_and_swap 함수 구현 및 worker 함수 내에 스핀락을 이용해 수정하여 동기화문제 해결 완료
 * 2023.05.10 컴퓨터학부 2019033936 이승섭 - compare_and_swap 함수 대신 stdatomic.h 라이브러리에 있는 CAE 방식을 이용해 worker 함수 내에 스핀락을 이용해 동기화문제 해결 완료
 */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>


#define N 8             /* 스레드 개수 */
#define RUNTIME 100000  /* 출력량을 제한하기 위한 실행시간 (마이크로초) */

/*
 * ANSI 컬러 코드: 출력을 쉽게 구분하기 위해서 사용한다.
 * 순서대로 BLK, RED, GRN, YEL, BLU, MAG, CYN, WHT, RESET을 의미한다.
 */
char *color[N+1] = {"\e[0;30m","\e[0;31m","\e[0;32m","\e[0;33m","\e[0;34m","\e[0;35m","\e[0;36m","\e[0;37m","\e[0m"};

/*
 * waiting[i]는 스레드 i가 임계구역에 들어가기 위해 기다리고 있음을 나타낸다.
 * alive 값이 false가 될 때까지 스레드 내의 루프가 무한히 반복된다.
 */
bool waiting[N];
bool alive = true;
atomic_int lock = 0; // lock을 위한 변수로 atomic_compare_exchange_weak()의 첫번째 인자로 들어가야하는데 원자적으로 들어가야하므려 atomic_int를 사용
int j; // 다음 스레드를 지정해주기 위한 변수

/*
 * N 개의 스레드가 임계구역에 배타적으로 들어가기 위해 스핀락을 사용하여 동기화한다.
 */

void *worker(void *arg)
{
    int i = *(int *)arg;
    
    
    while (alive) {
        /*
         * 임계구역: 알파벳 문자를 한 줄에 40개씩 10줄 출력한다.
         */
        waiting[i] = true; //waiting[i]는 다른 프로세스가 임계구역에 있으니 기다려야 한다.
        int expected = 0; // 변수 lock 과 같은지 아닌지 판단하기 위한 변수
        /*
         * 스레드가 하나씩 들어가야하므로 waiting큐에 스레드를 넣어주고 하나씩 빼면서 사용
         * expected 값을 false 로 설정하고 atomic_compare_exchange_weak() 함수를 이용해 스핀락을 건다.
         */
        while (waiting[i] && !(atomic_compare_exchange_weak(&lock, &expected, 1)))
            expected = 0;
        waiting[i] = false;
        /* critical section */
        for (int k = 0; k < 400; ++k) {
            printf("%s%c%s", color[i], 'A'+i, color[N]);
            if ((k+1) % 40 == 0)
                printf("\n");
        } /* critical section 종료*/
        /*
         * 스레드 i가 CS를 빠져나올때, 다음에 들어갈 스레드를 결정해준다.
         */
        j = (i + 1) % N;
        while ((j != i) && !waiting[j])
            j = (j + 1) % N;
        if (j == i)
            lock = 0; // 기다리는 프로세스가 없으므로 lock을 푼다.
        else
            waiting[j] = false;
        /*
         * 임계구역이 성공적으로 종료되었다.
         */
    }
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * N 개의 자식 스레드를 생성한다.
     */
    for (i = 0; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, worker, id+i);
    }
    /*
     * 스레드가 출력하는 동안 RUNTIME 마이크로초 쉰다.
     * 이 시간으로 스레드의 출력량을 조절한다.
     */
    usleep(RUNTIME);
    /*
     * 스레드가 자연스럽게 무한 루프를 빠져나올 수 있게 한다.
     */
    alive = false;
    /*
     * 자식 스레드가 종료될 때까지 기다린다.
     */
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
    /*
     * 메인함수를 종료한다.
     */
    return 0;
}
