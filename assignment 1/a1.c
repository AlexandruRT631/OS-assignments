#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> 

#pragma pack(push,1)
typedef struct section_header {
    char sect_name[16];
    unsigned short sect_type;
    unsigned int sect_offset;
    unsigned int sect_size;
}SECTION_HEADER;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct header{
    char magic[4];
    unsigned short header_size;
    unsigned char version;
    unsigned char no_of_sections;
    SECTION_HEADER section_header[10];
}HEADER;
#pragma pack(pop)

int search_path(char *dir_name, int recursive, char *name_starts_with, int has_perm_write) {
    int timesFound = 0;
    DIR* dir = opendir("./");
    if (dir == NULL) {
        return 0;
    }
    char oldDir[255];
    getcwd(oldDir, 255);
    struct dirent* dir_content = readdir(dir);
    while (dir_content != NULL) {
        if (strcmp(dir_content->d_name, "..") != 0 && strcmp(dir_content->d_name, ".") != 0){
            timesFound++;
            char path[255];
            strcpy(path, dir_name);
            strcat(path, "/");
            strcat(path, dir_content->d_name);
            chdir(dir_content->d_name);
            struct stat metadata;
            // I don't know why but both stat need to be here to work. 
            stat("./", &metadata);
            stat(dir_content->d_name, &metadata);
            if ((name_starts_with[0] == 0 || strncmp(dir_content->d_name, name_starts_with, strlen(name_starts_with)) == 0) && 
                (has_perm_write == 0 || (metadata.st_mode & S_IWUSR) || S_ISDIR(metadata.st_mode))) {
                printf("%s\n", path);
            }

            if (S_ISDIR(metadata.st_mode) && recursive == 1) {
                char newDir[255];
                getcwd(newDir, 255);
                if (!strstr(oldDir, newDir)) {
                    timesFound = timesFound + search_path(path, recursive, name_starts_with, has_perm_write);
                }
            }
            chdir(oldDir);
        }
        dir_content = readdir(dir);
    }
    closedir(dir);
    return timesFound;
}

int parseHeader(HEADER header, int print) {
    if (strncmp(header.magic, "sCHI", 4) != 0) {
        if (print == 1){
            printf("ERROR\nwrong magic\n");
        }
        return 1;
    }
    if (!(header.version >=84 && header.version <= 166)) {
        if (print == 1){
            printf("ERROR\nwrong version\n");
        }
        return 1;
    }
    if (!(header.no_of_sections >=2 && header.no_of_sections <= 10)) {
        if (print == 1){
            printf("ERROR\nwrong sect_nr\n");
        }
        return 1;
    }
    for (int i = 0; i < header.no_of_sections; i++) {
        SECTION_HEADER section_header = header.section_header[i];
        if (!(section_header.sect_type == 52 || section_header.sect_type == 12 || section_header.sect_type == 68)) {
            if (print == 1){
                printf("ERROR\nwrong sect_types\n");
            }
            return 1;
        }
    }
    if (print == 1){
        printf("SUCCESS\n");
        printf("version=%d\n", (int)header.version);
        printf("nr_sections=%d\n", (int)header.no_of_sections);
        for (int i = 0; i < header.no_of_sections; i++) {
            SECTION_HEADER section_header = header.section_header[i];
            printf("section%d: %.16s %d %d\n", i + 1, section_header.sect_name, section_header.sect_type, section_header.sect_size);
        }
    }
    return 0;
}

int printSectionLine(char *path, int section, int line) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("ERROR\ninvalid path\n");
        close(fd);
        return 1;
    }
    HEADER header = {0};
    read(fd, &header, sizeof(HEADER));
    if (header.no_of_sections < section) {
        printf("ERROR\ninvalid section\n");
        return 1;
    }
    SECTION_HEADER section_header = header.section_header[section - 1];
    lseek(fd, section_header.sect_offset, SEEK_SET);
    int i = 0;
    char buf[65535];
    int bufPoz = 0;
    read(fd, buf, 65535);
    int byte = 0;
    while(i < line - 1 && byte < section_header.sect_size) {
        if (buf[bufPoz] == '\n') {
            i++;
        }
        bufPoz++;
        byte++;
        if (bufPoz == 65535){
            read(fd, buf, 65535);
            bufPoz = 0;
        }
    }
    if (byte == section_header.sect_size) {
        printf("ERROR\ninvalid line\n");
        return 1;
    }
    printf("SUCCESS\n");
    while (buf[bufPoz] != '\n' && byte < section_header.sect_size) {
        printf("%c", buf[bufPoz]);
        bufPoz++;
        byte++;
        if (bufPoz == 65535){
            read(fd, buf, 65535);
            bufPoz = 0;
        }
    }
    close(fd);
    return 0;
}

int findAll(char *dir_name) {
    int timesFound = 0;
    DIR* dir = opendir("./");
    if (dir == NULL) {
        return 0;
    }
    char oldDir[255];
    getcwd(oldDir, 255);
    struct dirent* dir_content = readdir(dir);
    while (dir_content != NULL) {
        if (strcmp(dir_content->d_name, "..") != 0 && strcmp(dir_content->d_name, ".") != 0){
            timesFound++;
            char path[255];
            strcpy(path, dir_name);
            strcat(path, "/");
            strcat(path, dir_content->d_name);
            chdir(dir_content->d_name);
            struct stat metadata;
            // I don't know why but both stat need to be here to work. 
            stat("./", &metadata);

            int fd = open(dir_content->d_name, O_RDONLY);
            if (fd != -1) {
                int goodSections = 0;
                HEADER header = {0};
                read(fd, &header, sizeof(HEADER));
                if (parseHeader(header, 0) == 0){
                    SECTION_HEADER section_header;
                    for (int i = 0; i < header.no_of_sections && goodSections != 2; i++) {
                        section_header = header.section_header[i];
                        lseek(fd, section_header.sect_offset, SEEK_SET);
                        char buf[65535];
                        int bufPoz = 0;
                        int line = 1;
                        read(fd, buf, 65535);
                        int tooManyLines = 0;
                        for (int byte = 0; byte < section_header.sect_size && tooManyLines == 0; byte++) {
                            if (buf[bufPoz] == '\n') {
                                line++;
                            }
                            if (line > 14) {
                                tooManyLines = 1;
                            }
                            bufPoz++;
                            if (bufPoz == 65535){
                                read(fd, buf, 65535);
                                bufPoz = 0;
                            }
                        }
                        if (line == 14) {
                            goodSections++;
                        }
                        if (goodSections == 2) {
                            printf("%s\n", path);
                        }
                    }
                }
            }
            close(fd);
            
            if (S_ISDIR(metadata.st_mode)) {
                char newDir[255];
                getcwd(newDir, 255);
                if (!strstr(oldDir, newDir)) {
                    timesFound = timesFound + findAll(path);
                }
            }
            chdir(oldDir);
        }
        dir_content = readdir(dir);
    }
    closedir(dir);
    return timesFound;
}

int main(int argc, char **argv){
    if(argc >= 2){
        char command[255]; strcpy(command, "");
        char path[255]; strcpy(path, "");
        char name_starts_with[255]; strcpy(name_starts_with, "");
        int has_perm_write = 0;
        int recursive = 0;
        int section = -1;
        int line = -1;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "variant") == 0 || strcmp(argv[i], "list") == 0 || strcmp(argv[i], "parse") == 0 || strcmp(argv[i], "extract") == 0 || strcmp(argv[i], "findall") == 0) {
                strcpy(command, argv[i]);
            }
            else if (strncmp(argv[i], "path=", 5) == 0) {
                for (int j = 5; j < strlen(argv[i]); j++) {
                    path[j - 5] = argv[i][j];
                }
                path[strlen(argv[i]) - 5] = 0;
            }
            else if (strncmp(argv[i], "name_starts_with=", 17) == 0) {
                for (int j = 17; j < strlen(argv[i]); j++) {
                    name_starts_with[j - 17] = argv[i][j];
                }
                name_starts_with[strlen(argv[i]) - 17] = 0;
            }
            else if (strcmp(argv[i], "has_perm_write") == 0) {
                has_perm_write = 1;
            }
            else if (strcmp(argv[i], "recursive") == 0) {
                recursive = 1;
            }
            else if (strncmp(argv[i], "section=", 8) == 0) {
                section = 0;
                for (int j = 8; j < strlen(argv[i]); j++) {
                    section = section * 10 + argv[i][j] - '0';
                }
            }
            else if (strncmp(argv[i], "line=", 5) == 0) {
                line = 0;
                for (int j = 5; j < strlen(argv[i]); j++) {
                    line = line * 10 + argv[i][j] - '0';
                }
            }
            else {
                printf("ERROR\nunknown argument \"%s\"\n",argv[i]);
                return 1;
            }
        }

        // assignment 1
        if(strcmp(command, "variant") == 0){
            printf("92680\n");
        }
        // assignment 2
        else if (strcmp(command, "list") == 0){
            if (strcmp(path, "") != 0){
                DIR* dir = opendir(path);
                if (dir == NULL) {
                    printf("ERROR\ninvalid directory path\n");
                    closedir(dir);
                    return 1;
                }
                closedir(dir);
                printf("SUCCESS\n");
                chdir(path);
                search_path(path, recursive, name_starts_with, has_perm_write);
            }
            else {
                printf("ERROR\ninvalid directory path\n");
                return 1;
            }
        }
        // assignment 3
        else if (strcmp(command, "parse") == 0) {
            if (strcmp(path, "") != 0) {
                int fd = open(path, O_RDONLY);
                if (fd == -1) {
                    printf("ERROR\ninvalid path\n");
                    close(fd);
                    return 1;
                }
                HEADER header = {0};
                read(fd, &header, sizeof(HEADER));
                close(fd);
                parseHeader(header, 1);
            }
            else {
                printf("ERROR\ninvalid path\n");
                return 1;
            }
        }
        // assignment 4
        else if (strcmp(command, "extract") == 0) {
            if (strcmp(path, "") == 0) {
                printf("ERROR\ninvalid file\n");
                return 1;
            }
            if (section < 1) {
                printf("ERROR\ninvalid section\n");
                return 1;
            }
            if (line < 1) {
                printf("ERROR\ninvalid line\n");
                return 1;
            }
            printSectionLine(path, section, line);
        }
        // assignment 5
        else if (strcmp(command, "findall") == 0) {
            if (strcmp(path, "") == 0) {
                printf("ERROR\ninvalid directory path\n");
                return 1;
            }
            DIR* dir = opendir(path);
            if (dir == NULL) {
                printf("ERROR\ninvalid directory path\n");
                closedir(dir);
                return 1;
            }
            closedir(dir);
            printf("SUCCESS\n");
            chdir(path);
            findAll(path);
        }
        else {
            printf("ERROR\nno command specified!\n");
            return 1;
        }
    }
    else {
        printf("ERROR\nno argument specified!\n");
        return 1;
    }
    return 0;
}