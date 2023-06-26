/*
 * Copyright(c) 2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * 2023.03.19 컴퓨터학부 2019033936 이승섭 - 코드 분석 및 표준 입출력 리다이렉션 공부 시작
 * 참고 자료 - https://www.it-note.kr/19 (open함수에 어떤 파라미터를 넣어야 하는지 참고)
 * 참고 자료 - https://sosal.kr/1114?category=227313 (표준 입출력 리다이렉션 구현을 위해 해당 자료를 참고)
 * 2023.03.22 컴퓨터학부 2019033936 이승섭 - 표준 입출력 리다이렉션 구현 성공
 * 2023.03.22 컴퓨터학부 2019033936 이승섭 - 표준 입출력 리다이렉션 예외처리 (107번 코드 ~ 114번 코드)
 * 2023.03.22 컴퓨터학부 2019033936 이승섭 - 파이프 공부 시작
 * 참고 자료 - https://reakwon.tistory.com/80 (파이프라인 작동 원리 및 구조 이해를 위해 해당 자료를 참고)
 * 2023.03.23 컴퓨터학부 2019033936 이승섭 - 파이프 구현 시작
 * 참고 자료 - https://nomad-programmer.tistory.com/110 (pipe 함수 사용법 및 예제를 통한 이해)
 * 참고 자료 - https://medium.com/pocs/%EB%A6%AC%EB%88%85%EC%8A%A4-%EC%BB%A4%EB%84%90-%EC%9A%B4%EC%98%81%EC%B2%B4%EC%A0%9C-%EA%B0%95%EC%9D%98%EB%85%B8%ED%8A%B8-3-9ed24cf457ce (fork 작동원리를 이해하기 위해 해당 자료를 참고)
 * 2023.03.26 컴퓨터학부 2019033936 이승섭 - 파이프 구현 성공
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */

/*
 * cmdexec - 명령어를 파싱해서 실행한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다. 
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾸거나,
 * 기호 '|'를 사용하여 파이프 명령을 실행하는 것도 여기에서 처리한다.
 */
static void cmdexec(char *cmd)
{
    char *argv[MAX_LINE/2+1];   /* 명령어 인자를 저장하기 위한 배열 */
    int argc = 0;               /* 인자의 개수 */
    char *p, *q;                /* 명령어를 파싱하기 위한 변수 */
    int fd;                     /* 열린 파일의 서술자 */
    int pipe_fd[2];             /* pipe를 생성하기 위한 변수 */
    int status;                 /* 프로세스 상태 */
    pid_t pid;                  /* 자식 프로세스 아이디 */

    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */
    p = cmd; p += strspn(p, " \t");
    do {
        /*
         * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
         */ 
        q = strpbrk(p, " \t\'\"<>|");
        /*
         * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
         */
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t");
            if (*q) argv[argc++] = q;
        }
        /*
         * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else if (*q == '\'') {
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
        }
        /*
         * 표준 입력 리다이렉션 구현 ('<')
         */
        else if (*q == '<'){
            /*
             * 문자열 p에서 '<'를 제거한다.
             */
            q = strsep(&p, "<");
            /*
             * 문자열 p앞의 공백을 제거한다.
             */
            p += strspn(p, " \t");
            /*
             * 문자열 p로 된 파일을 읽기 전용 모드로 연다.
             */
            fd = open(p, O_RDONLY);
            /*
             * 표준 입력을 fd와 공유한다.
             */
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        /*
         * 표준 출력 리다이렉션 구현 ('>')
         */
        else if (*q == '>'){
            /*
             * 문자열 p에서 '>'를 제거한다.
             */
            q = strsep(&p, ">");
            /*
             * 문자열 p앞의 공백을 제거한다.
             */
            p += strspn(p, " \t");
            /*
             * 문자열의 길이를 수정하여 토큰 끝에 있는 공백을 제거한다.
             */
            int p_len = strlen(p);
            while(p[p_len-1] == ' ' || p[p_len-1] == '\t')
                p_len--;
            p[p_len] = '\0';
            /*
             * 문자열 p로 된 파일을 연다. 만약 파일이 없다면 문자열 p이름을 가진 파일을 생성한다.
             */
            fd = open(p, O_WRONLY | O_CREAT, 0644);
            /*
             * 표준 출력을 fd와 공유한다.
             */
            dup2(fd, STDOUT_FILENO);
            close(fd);
            /*
             * '>' 이후에 나온 문자열을 모두 처리해주었으므로 *p를 널값으로 설정하여
             출력 리다이렉션이 '>' 앞에 나온 명령에만 적용되도록 한다.
             */
            *p = '\0';
        }
        /*
         * 파이프 구현
         */
        else if (*q == '|'){
            /*
             * 문자열 p에서 '|'를 제거한다.
             */
            q = strsep(&p, "|");
            /*
             * 문자열 p 앞의 공백을 제거한다.
             */
            p += strspn(p, " \t");
            /*
             * pipe를 생성한다. -1을 반환하면 pipe() 가 실패했으므로 exit(1)로 종료한다.
             */
            if (pipe(pipe_fd) == -1){
                printf("Pipe failed\n");
                exit(1);
            }
            /*
             * 손자 프로세스를 생성한다. -1을 반환하면 손자 프로세스 생성이 실패했으므로 exit(1)로 종료한다.
             */
            if ((pid = fork()) == -1){
                printf("Fork failed\n");
                exit(1);
            }
            /*
             * 손자 프로세스는 명령어를 실행하고 종료한다.
             * 표준 출력을 pipe_fd[1]와 공유한다.
             */
            if (pid == 0) {
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                *p = '\0';
            }
            /*
             * 자식 프로세스는 명령어를 실행하고 종료한다.
             * 표준 입력을 pipe_fd[0]과 공유한다.
             * 손자 프로세스가 종료된 다음에 종료 상태를 가져온다.
             * 재귀적으로 코드를 처리해 여러개의 파이프 ('|')를 처리할 수 있다.
             */
            else{
                close(pipe_fd[1]);
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);
                wait(&status);
                cmdexec(p);
            }
        }

        /*
         * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고,
         * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else {
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }        
    } while (p);
    argv[argc] = NULL;
    /*
     * argv에 저장된 명령어를 실행한다.
     */
    if (argc > 0)
        execvp(argv[0], argv);
}

/*
 * 기능이 간단한 유닉스 셸인 tsh (tiny shell)의 메인 함수이다.
 * tsh은 프로세스 생성과 파이프를 통한 프로세스간 통신을 학습하기 위한 것으로
 * 백그라운드 실행, 파이프 명령, 표준 입출력 리다이렉션 일부만 지원한다.
 */
int main(void)
{
    char cmd[MAX_LINE+1];       /* 명령어를 저장하기 위한 버퍼 */
    int len;                    /* 입력된 명령어의 길이 */
    pid_t pid;                  /* 자식 프로세스 아이디 */
    int background;             /* 백그라운드 실행 유무 */
    
    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true) {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */
        printf("tsh> "); fflush(stdout);
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if(!strcasecmp(cmd, "exit"))
            break;
        /*
         * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
         */
        char *p = strchr(cmd, '&');
        if (p != NULL) {
            background = 1;
            *p = '\0';
        }
        else
            background = 0;
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0) {
            cmdexec(cmd);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
        else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}
