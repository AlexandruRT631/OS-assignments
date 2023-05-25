#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include "a2_helper.h"

int process_sem;

int p2_sem;

int p5_sem;
int p5_started_th = 0;

int p2_p6_sem;

void P(int sem_id, int sem_no)
{
    struct sembuf op = {sem_no, -1, 0};
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_no)
{
    struct sembuf op = {sem_no, +1, 0};
    semop(sem_id, &op, 1);
}

void p2t1()
{
    P(p2_sem, 0);
    info(BEGIN, 2, 1);
    info(END, 2, 1);
    V(p2_sem, 1);
}

void p2t2()
{
    info(BEGIN, 2, 2);
    info(END, 2, 2);
}

void p2t3()
{
    P(p2_p6_sem, 0);
    info(BEGIN, 2, 3);
    info(END, 2, 3);
    V(p2_p6_sem, 1);
}

void p2t4()
{
    info(BEGIN, 2, 4);
    V(p2_sem, 0);
    P(p2_sem, 1);
    info(END, 2, 4);
}

void p5t12()
{
    // thread 12 starts
    P(p5_sem, 0);
    info(BEGIN, 5, 12);
    p5_started_th++;
    // announce thread 12 has fully started
    V(p5_sem, 4);
    // wait for 6 threads to be concurrently open
    P(p5_sem, 2);
    info(END, 5, 12);
    V(p5_sem, 0);
    // announce thread 12 has fully finished
    V(p5_sem, 3);
}

void p5toth(int th)
{
    P(p5_sem, 0);
    info(BEGIN, 5, th);
    P(p5_sem, 1);
    p5_started_th++;
    if (p5_started_th == 6)
    {
        // announce thread 12 that 6 threads are concurrently open
        V(p5_sem, 2);
    }
    V(p5_sem, 1);
    // wait for thread 12 to fully finish
    P(p5_sem, 3);
    V(p5_sem, 3);
    info(END, 5, th);
    V(p5_sem, 0);
}

void p6t2()
{
    P(p2_p6_sem, 1);
    info(BEGIN, 6, 2);
    info(END, 6, 2);
}

void p6t4()
{
    info(BEGIN, 6, 4);
    info(END, 6, 4);
    V(p2_p6_sem, 0);
}

void p6toth(int th)
{
    info(BEGIN, 6, th);
    info(END, 6, th);
}

int main()
{
    init();

    // P1
    info(BEGIN, 1, 0);

    process_sem = semget(IPC_PRIVATE, 7, IPC_CREAT | 0600);
    for (int i = 0; i <= 7; i++)
    {
        semctl(process_sem, i, SETVAL, 0);
    }

    // semaphore between p2 and p6
    p2_p6_sem = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
    semctl(p2_p6_sem, 0, SETVAL, 0);
    semctl(p2_p6_sem, 1, SETVAL, 0);

    if (fork() == 0) // P2
    {
        info(BEGIN, 2, 0);

        semctl(process_sem, 2, SETVAL, 0);
        semctl(process_sem, 3, SETVAL, 0);

        if (fork() == 0) // P4
        {
            info(BEGIN, 4, 0);

            semctl(process_sem, 4, SETVAL, 0);

            if (fork() == 0) // P6
            {
                info(BEGIN, 6, 0);

                semctl(process_sem, 6, SETVAL, 0);

                if (fork() == 0) // P7
                {
                    info(BEGIN, 7, 0);

                    info(END, 7, 0);
                    // signal P7 has finished
                    V(process_sem, 6);
                }
                else
                {
                    pthread_t th[5];
                    // thread 2
                    if (pthread_create(&th[1], NULL, (void *(*)(void *))p6t2, NULL) != 0)
                    {
                        printf("Cannot create threads!\n");
                        return 1;
                    }
                    // thread 4
                    if (pthread_create(&th[3], NULL, (void *(*)(void *))p6t4, NULL) != 0)
                    {
                        printf("Cannot create threads!\n");
                        return 1;
                    }
                    for (int i = 0; i < 5; i++)
                    {
                        // not thread 2 or 4
                        if (i != 1 && i != 3)
                        {
                            if (pthread_create(&th[i], NULL, (void *(*)(void *))p6toth, (void *)(long)(i + 1)) != 0)
                            {
                                printf("Cannot create threads!\n");
                                return 1;
                            }
                        }
                    }

                    // waiting for the threads to finish
                    for (int i = 0; i < 5; i++)
                    {
                        pthread_join(th[i], NULL);
                    }

                    P(process_sem, 6);

                    info(END, 6, 0);
                    // signal P6 has finished
                    V(process_sem, 4);
                }
            }
            else
            {
                // waiting for P6 to finish
                P(process_sem, 4);

                info(END, 4, 0);
                // signal P4 has finished
                V(process_sem, 2);
            }
        }
        else if (fork() == 0) // P5
        {
            info(BEGIN, 5, 0);

            semctl(process_sem, 5, SETVAL, 0);

            if (fork() == 0) // P8
            {
                info(BEGIN, 8, 0);

                info(END, 8, 0);
                // signal P8 has finished
                V(process_sem, 5);
            }
            else
            {
                p5_sem = semget(IPC_PRIVATE, 5, IPC_CREAT | 0600);
                semctl(p5_sem, 0, SETVAL, 6);
                semctl(p5_sem, 1, SETVAL, 1);
                semctl(p5_sem, 2, SETVAL, 0);
                semctl(p5_sem, 3, SETVAL, 0);
                semctl(p5_sem, 4, SETVAL, 0);

                pthread_t th[39];
                // thread 12
                if (pthread_create(&th[11], NULL, (void *(*)(void *))p5t12, NULL) != 0)
                {
                    printf("Cannot create threads!\n");
                    return 1;
                }
                // wait for thread 12 to finish the starting procedure
                P(p5_sem, 4);
                for (int i = 0; i < 39; i++)
                {
                    // not thread 12
                    if (i != 11)
                    {
                        if (pthread_create(&th[i], NULL, (void *(*)(void *))p5toth, (void *)(long)(i + 1)) != 0)
                        {
                            printf("Cannot create threads!\n");
                            return 1;
                        }
                    }
                }

                // waiting for the threads to finish
                for (int i = 0; i < 39; i++)
                {
                    pthread_join(th[i], NULL);
                }

                // waiting for P8 to finish
                P(process_sem, 5);

                info(END, 5, 0);
                // signal P5 has finished
                V(process_sem, 3);
            }
        }
        else
        {
            p2_sem = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
            semctl(p2_sem, 0, SETVAL, 0);
            semctl(p2_sem, 1, SETVAL, 0);

            pthread_t th[4];
            if (pthread_create(&th[0], NULL, (void *(*)(void *))p2t1, NULL) != 0 ||
                pthread_create(&th[1], NULL, (void *(*)(void *))p2t2, NULL) != 0 ||
                pthread_create(&th[2], NULL, (void *(*)(void *))p2t3, NULL) != 0 ||
                pthread_create(&th[3], NULL, (void *(*)(void *))p2t4, NULL) != 0)
            {
                printf("Cannot create threads!\n");
                return 1;
            }

            // waiting for the threads to finish
            for (int i = 0; i < 4; i++)
            {
                pthread_join(th[i], NULL);
            }

            // waiting for P4 and P5 to finish
            P(process_sem, 2);
            P(process_sem, 3);

            info(END, 2, 0);
            // signal P2 has finished
            V(process_sem, 0);
        }
    }
    else if (fork() == 0) // P3
    {
        info(BEGIN, 3, 0);

        info(END, 3, 0);
        // signal P3 has finished
        V(process_sem, 1);
    }
    else
    {
        // waiting for P2 and P3 to finish
        P(process_sem, 0);
        P(process_sem, 1);

        info(END, 1, 0);
    }
    return 0;
}
