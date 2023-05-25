#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
    if (mkfifo("RESP_PIPE_92680", 0600) < 0)
    {
        printf("ERROR\ncannot create the response pipe\n");
        return 1;
    }
    int fdR = open("REQ_PIPE_92680", O_RDONLY);
    if (fdR < 0)
    {
        printf("ERROR\ncannot open the request pipe\n");
        unlink("RESP_PIPE_92680");
        return 1;
    }
    int fdW = open("RESP_PIPE_92680", O_WRONLY);
    if (fdW < 0)
    {
        printf("ERROR\ncannot open the response pipe\n");
        close(fdR);
        unlink("RESP_PIPE_92680");
        return 1;
    }

    char buf[255];
    buf[0] = 7; buf[1] = 0; strcat(buf, "CONNECT");
    write(fdW, buf, strlen(buf));
    printf("SUCCESS\n");

    char *shm = NULL;
    unsigned int shm_size;
    char *mapData = NULL;
    int mapSize;

    while (1)
    {
        read(fdR, buf, 1);
        int bufSize = (int)buf[0];
        int n = read(fdR, buf, bufSize);
        buf[n] = 0;
        if (strcmp(buf, "PING") == 0)
        {
            buf[0] = 4; buf[1] = 0; strcat(buf, "PING");
            buf[5] = 4; buf[6] = 0; strcat(buf, "PONG");
            int x = 92680;
            write(fdW, buf, 10);
            write(fdW, &x, sizeof(unsigned int));
        }
        else if (strcmp(buf, "CREATE_SHM") == 0)
        {
            read(fdR, &shm_size, sizeof(unsigned int));
            int shm_fd = shm_open("/pWWigb", O_CREAT | O_RDWR, 0664);
            if (shm_fd < 0)
            {
                buf[0] = 10; buf[1] = 0; strcat(buf, "CREATE_SHM");
                buf[11] = 5; buf[12] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 17); 
                continue;
            }
            ftruncate(shm_fd, shm_size);
            shm = (char *)mmap(NULL, sizeof(char) * shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (shm == MAP_FAILED)
            {
                buf[0] = 10; buf[1] = 0; strcat(buf, "CREATE_SHM");
                buf[11] = 5; buf[12] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 17);
                continue;
            }
            close(shm_fd);
            buf[0] = 10; buf[1] = 0; strcat(buf, "CREATE_SHM");
            buf[11] = 7; buf[12] = 0; strcat(buf, "SUCCESS");
            write(fdW, buf, 19);
        }
        else if (strcmp(buf, "WRITE_TO_SHM") == 0)
        {
            unsigned int offset;
            read(fdR, &offset, sizeof(unsigned int));
            read(fdR, buf, 4);
            if (offset + 4 > shm_size)
            {
                buf[0] = 12; buf[1] = 0; strcat(buf, "WRITE_TO_SHM");
                buf[13] = 5; buf[14] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 19);
                continue;
            }
            for (int i = 3; i >= 0; i--)
            {
                *(shm + offset + i) = buf[i];
            }
            buf[0] = 12; buf[1] = 0; strcat(buf, "WRITE_TO_SHM");
            buf[13] = 7; buf[14] = 0;  strcat(buf, "SUCCESS");
            write(fdW, buf, 21);
        }
        else if (strcmp(buf, "MAP_FILE") == 0)
        {
            read(fdR, buf, 1);
            int fileNameSize = (int)buf[0];
            n = read(fdR, buf, fileNameSize);
            buf[n] = 0;
            int fd = open(buf, O_RDONLY);
            if (fd < 0)
            {
                buf[0] = 8; buf[1] = 0; strcat(buf, "MAP_FILE");
                buf[9] = 5; buf[10] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 15);
                continue;
            }
            mapSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            mapData = (char *)mmap(NULL, mapSize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapData == MAP_FAILED)
            {
                buf[0] = 8; buf[1] = 0; strcat(buf, "MAP_FILE");
                buf[9] = 5; buf[10] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 15);
                continue;
            }
            close(fd);
            buf[0] = 8; buf[1] = 0; strcat(buf, "MAP_FILE");
            buf[9] = 7; buf[10] = 0;  strcat(buf, "SUCCESS");
            write(fdW, buf, 17);
        }
        else if (strcmp(buf, "READ_FROM_FILE_OFFSET") == 0)
        {
            unsigned int offset;
            read(fdR, &offset, sizeof(unsigned int));
            unsigned int no_of_bytes;
            read(fdR, &no_of_bytes, sizeof(unsigned int));
            if (shm == NULL || mapData == NULL || offset + no_of_bytes >= mapSize || no_of_bytes >= shm_size)
            {
                buf[0] = 21; buf[1] = 0; strcat(buf, "READ_FROM_FILE_OFFSET");
                buf[22] = 5; buf[23] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 28);
                continue;
            }

            for (int i = 0; i < no_of_bytes; i++)
            {
                *(shm + i) = mapData[offset + i];
            }
            buf[0] = 21; buf[1] = 0; strcat(buf, "READ_FROM_FILE_OFFSET");
            buf[22] = 7; buf[23] = 0; strcat(buf, "SUCCESS");
            write(fdW, buf, 30);
        }
        else if (strcmp(buf, "READ_FROM_FILE_SECTION") == 0)
        {
            unsigned int section_no;
            read(fdR, &section_no, sizeof(unsigned int));
            unsigned int offset;
            read(fdR, &offset, sizeof(unsigned int));
            unsigned int no_of_bytes;
            read(fdR, &no_of_bytes, sizeof(unsigned int));
            unsigned int no_of_sections = (unsigned int)mapData[7];
            if (section_no > no_of_sections) {
                buf[0] = 22; buf[1] = 0; strcat(buf, "READ_FROM_FILE_SECTION");
                buf[23] = 5; buf[24] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 29);
                continue;
            }
            unsigned int sect_offset = 0;
            int aux = 1;
            for (int i = 0; i < 4; i++)
            {
                sect_offset = sect_offset + (unsigned char)mapData[8 + 26 * (section_no - 1) + 18 + i] * aux;
                aux = aux * 256;
            }
            unsigned int sect_size = 0;
            aux = 1;
            for (int i = 0; i < 4; i++)
            {
                sect_size = sect_size + (unsigned char)mapData[8 + 26 * (section_no - 1) + 22 + i] * aux;
                aux = aux * 256;
            }
            if (offset + no_of_bytes >= sect_size || no_of_bytes >= shm_size)
            {
                buf[0] = 22; buf[1] = 0; strcat(buf, "READ_FROM_FILE_SECTION");
                buf[23] = 5; buf[24] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 29);
                continue;
            }
            for (int i = 0; i < no_of_bytes; i++)
            {
                *(shm + i) = mapData[sect_offset + offset + i];
            }
            buf[0] = 22; buf[1] = 0; strcat(buf, "READ_FROM_FILE_SECTION");
            buf[23] = 7; buf[24] = 0; strcat(buf, "SUCCESS");
            write(fdW, buf, 31);
        }
        else if (strcmp(buf, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0)
        {
            unsigned int logical_offset;
            read(fdR, &logical_offset, sizeof(unsigned int));
            unsigned int no_of_bytes;
            read(fdR, &no_of_bytes, sizeof(unsigned int));
            int sectionOrder[10];
            int sectionOffset[10];
            int sectionSize[10];
            int no_of_sections = (int)mapData[7];
            int remainingOffset = logical_offset % 3072;
            for (int i = 0; i < no_of_sections; i++)
            {
                sectionOrder[i] = i;
                sectionOffset[i] = 0;
                int aux = 1;
                for (int j = 0; j < 4; j++)
                {
                    sectionOffset[i] = sectionOffset[i] + (unsigned char)mapData[8 + i * 26 + 18 + j] * aux;
                    aux = aux * 256;
                }
                sectionSize[i] = 0;
                aux = 1;
                for (int j = 0; j < 4; j++)
                {
                    sectionSize[i] = sectionSize[i] + (unsigned char)mapData[8 + i * 26 + 22 + j] * aux;
                    aux = aux * 256;
                }
            }
            for (int i = 0; i < no_of_sections - 1; i++)
            {
                for (int j = i + 1; j < no_of_sections; j++)
                {
                    if (sectionOffset[i] > sectionOffset[j])
                    {
                        int aux = sectionOrder[i];
                        sectionOrder[i] = sectionOrder[j];
                        sectionOrder[j] = aux;
                        aux = sectionOffset[i];
                        sectionOffset[i] = sectionOffset[j];
                        sectionOffset[j] = aux;
                        aux = sectionSize[i];
                        sectionSize[i] = sectionSize[j];
                        sectionSize[j] = aux;
                    }
                }
            }
            int section;
            int currentOffset = 0;
            for (section = 0; section < no_of_sections && currentOffset < logical_offset; section++)
            {
                currentOffset = currentOffset + 3072 * (sectionSize[section] / 3072 + 1);
            }
            section--;
            if (currentOffset < logical_offset || sectionSize[section] < remainingOffset + no_of_bytes)
            {
                buf[0] = 30; buf[1] = 0; strcat(buf, "READ_FROM_LOGICAL_SPACE_OFFSET");
                buf[31] = 5; buf[32] = 0; strcat(buf, "ERROR");
                write(fdW, buf, 37);
            }
            for (int i = 0; i < no_of_bytes; i++) {
                *(shm + i) = mapData[sectionOffset[section] + remainingOffset + i];
            }
            buf[0] = 30; buf[1] = 0; strcat(buf, "READ_FROM_LOGICAL_SPACE_OFFSET");
            buf[31] = 7; buf[32] = 0; strcat(buf, "SUCCESS");
            write(fdW, buf, 39);
        }
        else if (strcmp(buf, "EXIT") == 0)
        {
            break;
        }
    }
    close(fdR);
    close(fdW);
    unlink("RESP_PIPE_92680");
    munmap(mapData, mapSize);
    munmap(shm, sizeof(char) * shm_size);
    shm = NULL;
    shm_unlink("/pWWigb");
    return 0;
}