/*
 * Copyright(c) 2021-2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * 6월 2일 컴퓨터학부 2019033936 이승섭 - 코드 분석 및 스레드풀 공부 시작
 * 6월 2일 컴퓨터학부 2019033936 이승섭 - pthread_pool_init() 함수 구현 완료
 * 6월 2일 컴퓨터학부 2019033936 이승섭 - pthread_pool_submit() 함수 구현 완료
 * 6월 2일 컴퓨터학부 2019033936 이승섭 - pthread_pool_shutdown() 함수 구현 완료
 * 6월 3일 컴퓨터학부 2019033936 이승섭 - worker() 함수 구현 완료
 * 6월 3일 컴퓨터학부 2019033936 이승섭 - 데드락 발생 및 pthread_pool_shutdown() 함수 수정 완료
 * 6월 3일 컴퓨터학부 2019033936 이승섭 - 데드락 발생 및 pthread_pool_shutdown() 함수 수정 완료
 * 6월 3일 컴퓨터학부 2019033936 이승섭 - 코드 완성
 * 참고 자료 1 https://happytear.tistory.com/entry/Pthread-%EC%93%B0%EB%A0%88%EB%93%9C%ED%92%80-%EC%82%AC%EC%9A%A9%ED%95%98%EA%B8%B0 - 스레드풀 개념 이해
 * 참고 자료 2 https://popcorntree.tistory.com/67 - 스레드풀 코드 구현을 위해 도움을 받았음
 */
#include "pthread_pool.h"
#include <stdlib.h>

/*
 * 풀에 있는 일꾼(일벌) 스레드가 수행할 함수이다.
 * FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 * 대기열에 작업이 없으면 새 작업이 들어올 때까지 기다린다.
 * 이 과정을 스레드풀이 종료될 때까지 반복한다.
 */
static void *worker(void *param)
{
    //pthread_pool.h 에서 만들어진 구조체에 대한 포인터를 가져온다.
    pthread_pool_t *pool = (pthread_pool_t *)param;
    
    while (true) {
        pthread_mutex_lock(&pool->mutex);
        
        //대기열이 비어있고 스레드풀이 실행중인 동안에는 기다린다.
        while (pool->q_len == 0 && pool->running) {
            pthread_cond_wait(&pool->empty, &pool->mutex);
        }
        
        //스레드풀이 종료되면 스레드를 종료한다.
        if (!pool->running) {
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }

        //q_front를 1 증가시켜 다음 작업으로 이동시키고 대기열에 있는 작업의 개수를 감소시킨다.
        int index = pool->q_front;
        pool->q_front = (pool->q_front + 1) % pool->q_size;
        pool->q_len--;

        //q_len의 값이 q_size값보다 1만큼 작으면 자리가 생겼음을 뜻하므로 대기열이 꽉차 기다리고 있던 스레드에게 signal 을 보낸다.
        if (pool->q_len == pool->q_size - 1) {
            pthread_cond_broadcast(&pool->full);
        }
        pthread_mutex_unlock(&pool->mutex);

        //작업을 실행한다.
        task_t task = pool->q[index];
        task.function(task.param);
    }
}

/*
 * 스레드풀을 생성한다. bee_size는 일꾼(일벌) 스레드의 개수이고, queue_size는 대기열의 용량이다.
 * bee_size는 POOL_MAXBSIZE를, queue_size는 POOL_MAXQSIZE를 넘을 수 없다.
 * 일꾼 스레드와 대기열에 필요한 공간을 할당하고 변수를 초기화한다.
 * 일꾼 스레드의 동기화를 위해 사용할 상호배타 락과 조건변수도 초기화한다.
 * 마지막 단계에서는 일꾼 스레드를 생성하여 각 스레드가 worker() 함수를 실행하게 한다.
 * 대기열로 사용할 원형 버퍼의 용량이 일꾼 스레드의 수보다 작으면 효율을 극대화할 수 없다.
 * 이런 경우 사용자가 요청한 queue_size를 bee_size로 상향 조정한다.
 * 성공하면 POOL_SUCCESS를, 실패하면 POOL_FAIL을 리턴한다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
    //스레드풀 구조체 초기화를 해준다.
    pool->running = true;
    pool->q = (task_t *)malloc(queue_size * sizeof(task_t));
    pool->q_size = queue_size;
    pool->q_front = 0;
    pool->q_len = 0;
    pool->bee = (pthread_t *)malloc(bee_size * sizeof(pthread_t));
    pool->bee_size = bee_size;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->full, NULL);
    pthread_cond_init(&pool->empty, NULL);
    
    //스레드풀 생성에 실패했으므로 POOL_FAIL을 리턴한다.
    if (pool->q == NULL || pool->bee == NULL) {
        return POOL_FAIL;
    }
    
    //bee_size 또는 queue_size가 각각 지정된 크기를 넘었으므로 POOL_FAIL을 리턴한다.
    if (bee_size > POOL_MAXBSIZE || queue_size > POOL_MAXQSIZE)
        return POOL_FAIL;
    
    //대기열로 사용할 원형 버퍼의 용량이 일꾼 스레드의 수보다 작으면 queue_size를 bee_size로 상향 조정한다.
    if (queue_size < bee_size)
        queue_size = bee_size;
    
    //일꾼 스레드를 생성한다.
    for (int i = 0; i < bee_size; i++) {
        pthread_create(&pool->bee[i], NULL, worker, pool);
    }
    
    //스레드풀 생성에 성공했으므로 POOL_SUCCESS를 리턴한다.
    return POOL_SUCCESS;
}

/*
 * 스레드풀에서 실행시킬 함수와 인자의 주소를 넘겨주며 작업을 요청한다.
 * 스레드풀의 대기열이 꽉 찬 상황에서 flag이 POOL_NOWAIT이면 즉시 POOL_FULL을 리턴한다.
 * POOL_WAIT이면 대기열에 빈 자리가 나올 때까지 기다렸다가 넣고 나온다.
 * 작업 요청이 성공하면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag)
{
    pthread_mutex_lock(&pool->mutex);
    
    //스레드풀의 대기열이 꽉 찬 상황에서 flag의 값에 따라 처리 방식이 바뀐다.
    //flag 가 POOL_NOWAIT이면 즉시 POOL_FULL을 리턴하는데 이 때 뮤텍스락을 풀어준다.
    while (pool->q_len == pool->q_size) {
        if(flag == POOL_NOWAIT){
            pthread_mutex_unlock(&pool->mutex); //스레드풀 기본 동작 검증 데드락 해결 방법
            return POOL_FULL;
        }
        // flag가 POOL_WAIT이면 대기열에 빈 자리가 나올때까지 대기한다.
        else{
            pthread_cond_wait(&pool->full, &pool->mutex);
        }
    }
    
    //대기열에 작업을 추가해주며 q_len의 값을 1 증가시킨다.
    int index = (pool->q_front + pool->q_len) % pool->q_size;
    pool->q[index].function = f;
    pool->q[index].param = p;
    pool->q_len++;
    
    //대기 중인 일꾼 스레드에게 시그널을 보내 작업이 가능하다고 알린다.
    pthread_cond_signal(&pool->empty);
    pthread_mutex_unlock(&pool->mutex);
    
    //작업 요청이 성공했으므로 POOL_SUCCESS를 리턴한다.
    return POOL_SUCCESS;
}

/*
 * 스레드풀을 종료한다. 일꾼 스레드가 현재 작업 중이면 그 작업을 마치게 한다.
 * how의 값이 POOL_COMPLETE이면 대기열에 남아 있는 모든 작업을 마치고 종료한다.
 * POOL_DISCARD이면 대기열에 새 작업이 남아 있어도 더 이상 수행하지 않고 종료한다.
 * 부모 스레드는 종료된 일꾼 스레드와 조인한 후에 스레드풀에 할당된 자원을 반납한다.
 * 스레드를 종료시키기 위해 철회를 생각할 수 있으나 바람직하지 않다.
 * 락을 소유한 스레드를 중간에 철회하면 교착상태가 발생하기 쉽기 때문이다.
 * 종료가 완료되면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_shutdown(pthread_pool_t *pool, int how)
{
    pthread_mutex_lock(&pool->mutex);
    // 스레드풀을 종료한다.
    pool->running = false;
    // 일꾼 스레드가 현재 작업 중이면 그 작업을 마치게 한다.
    pthread_cond_broadcast(&pool->empty);
        
    // how 가 POOL_COMPLETE 이면 대기열에 남아 있는 모든 작업을 마치고 종료한다.
    if (how == POOL_COMPLETE) {
        while (pool->q_len > 0) {
            int index = pool->q_front;
            pool->q_front = (pool->q_front + 1) % pool->q_size;
            pool->q_len--;

            if (pool->q_len == pool->q_size - 1) {
                pthread_cond_broadcast(&pool->full);
            }
            
            task_t task = pool->q[index];
            task.function(task.param);
        }
    }
    // how 가 POOL_DISCARD 이면 대기열에 작업이 남이 있어도 대기열을 비워준다.
    else{
        pool->q_front = 0;
        pool->q_len = 0;
    }
    
        
    pthread_mutex_unlock(&pool->mutex);
    
    //종료된 일꾼 스레드와 조인한다.
    for (int i = 0; i < pool->bee_size; i++) {
        pthread_join(pool->bee[i], NULL);
    }

    // 사용한 자원을 해제한다.
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->full);
    pthread_cond_destroy(&pool->empty);
    
    //할당된 공간도 풀어준다.
    free(pool->q);
    free(pool->bee);
    
    //종료가 완료되었으므로 POOL_SUCCESS를 리턴한다.
    return POOL_SUCCESS;
}
