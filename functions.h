#include <dirent.h>
#include <limits.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

enum { MAX_DIR_NUM = 1024 };

struct File {
    char *name;
    size_t size;
    size_t type;
};

typedef struct File File;

char cwd[PATH_MAX];
File files[MAX_DIR_NUM];
size_t n_files = 0;
size_t current_file = 0;
struct dirent *pointed_directory = NULL;

size_t min(size_t a, size_t b);

size_t max_rows = 0;
size_t max_cols = 0;
size_t start_row;
size_t end_row;

size_t view_mod = 0;

FILE* copy_file_ptr = NULL;
FILE* paste_file_ptr;

char* copied_file_name;
char path_to_copied_file[PATH_MAX];
int cut = 0;

void handle_error(char buff[]) {
    endwin();
    perror(buff);
    exit(1);
}

void manage_color() {
    if (has_colors() == FALSE) {
        char error[] = "Your terminal does not support color\n";
        handle_error(error);
    }
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
}

#define DEFAULT_COLOR 2
#define CURRENT_FILE_COLOR 1
#define LINK_COLOR 3
#define FIFO_COLOR 4

void print_header() {
    printw("Current directory: %s\n", cwd);
    printw("~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~ \n");
}

static int file_comp(const void* a, const void* b) {
    return strcmp(((const File*)a)->name, ((const File*)b)->name);
}

int skip_dot(char* buf) {
    if (strlen(buf) == 1 && buf[0] == '.') {
        return 1;
    }
    return 0;
}

int go_upper(char *buf) {
    if (strlen(buf) == 2 && buf[0] == '.' && buf[1] == '.') {
        return 1;
    }
    return 0;
}

int is_hiden_directory(char *buf) {
    if (strlen(buf) > 1 && buf[0] == '.' && !go_upper(buf)) {
        return 1;
    }
    return 0;
}

void print_name_and_size(File* cur_file) {
    printw("%s", cur_file->name);
    for (size_t i = 0; i < 50 - strlen(cur_file->name); ++i) {
        printw(" ");
    }
    printw("%zu bytes\n", cur_file->size);
    refresh();
}

void print_directories() {
    DIR *current_directory = opendir(cwd);
    if (current_directory == NULL) {
        char error[] = "Error while opening directory\n";
        handle_error(error);
    }
    size_t prev_size = strlen(cwd);
    if (cwd[prev_size - 1] != '/') {
        cwd[prev_size++] = '/';
        cwd[prev_size] = '\0';
    }

    struct dirent *current;

    struct stat information;

    size_t n = 0;

    while ((current = readdir(current_directory)) != NULL) {
        if (skip_dot(current->d_name)) {
            continue;
        }
        if (view_mod == 0 && is_hiden_directory(current->d_name)) {
            continue;
        }
        files[n].name = current->d_name;
        if (current->d_type == DT_LNK) {
            files[n].type = 1;
        } else if (current->d_type == DT_FIFO) {
            files[n].type = 2;
        } else {
            files[n].type = 0;
        }

        prev_size = strlen(cwd);
        strcat(cwd, current->d_name);

        stat(cwd, &information);

        cwd[prev_size] = '\0';
        files[n].size = information.st_size;
        n++;
    }

    n_files = n;
    if (current_file == 0) {
        start_row = 0;
        end_row = min(n, max_rows);
    }


    qsort(files, n, sizeof(File), file_comp);

    for (size_t j = start_row; j < end_row; ++j) {
        if (j == current_file) {
            attron(COLOR_PAIR(CURRENT_FILE_COLOR));
        } else {
            if (files[j].type == 1) {
                attron(COLOR_PAIR(LINK_COLOR));
            } else if (files[j].type == 2) {
                attron(COLOR_PAIR(FIFO_COLOR));
            }
        }
        print_name_and_size(&files[j]);
        attroff(COLOR_PAIR(DEFAULT_COLOR));
    }

    char *tmp = files[current_file].name;
    closedir(current_directory);
    current_directory = opendir(cwd);
    while ((current = readdir(current_directory)) != NULL) {
        if (current->d_name == tmp) {
            pointed_directory = current;
            break;
        }
    }

    closedir(current_directory);
}

size_t min(size_t a, size_t b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

size_t max(size_t a, size_t b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

size_t find_previous_directory() {
    size_t size = strlen(cwd);
    for (size_t i = 1; i < size; ++i) {
        if (cwd[size - 1 - i] == '/') {
            return size - 1 - i;
        }
    }
    return 0;
}

void update() {
    clear();
    n_files = 0;
    current_file = 0;
    print_directories();
//    refresh();
}

void process_enter(struct dirent *current) {
    if (current->d_type == DT_DIR) {
//        refresh();
        size_t prev_size = strlen(cwd);
        if (cwd[prev_size - 1] != '/') {
            cwd[prev_size++] = '/';
            cwd[prev_size] = '\0';
        }
        if (go_upper(current->d_name)) {
            size_t pos = find_previous_directory();
            if (pos != 0) {
                cwd[pos] = '\0';
            }
        } else {
            strcat(cwd, current->d_name);
        }
        update();
    }
//    refresh();
}

void delete_file() {
    size_t prev_size = strlen(cwd);
    if (cwd[prev_size - 1] != '/') {
        cwd[prev_size++] = '/';
        cwd[prev_size] = '\0';
    }
    strcat(cwd, files[current_file].name);

    if (remove(cwd)) {
        char *buf = "can't remove file\n";
        perror(buf);
    }
    cwd[prev_size] = '\0';
    update();
}

void change_view_mod() {
    if (view_mod == 0) {
        view_mod = 1;
    } else {
        view_mod = 0;
    }
    update();
}

void copy_file() {
    if (copy_file_ptr != NULL) {
        fclose(copy_file_ptr);
        copy_file_ptr = NULL;
    }

    copied_file_name = files[current_file].name;

    size_t prev_size = strlen(cwd);
    if (cwd[prev_size - 1] != '/') {
        cwd[prev_size++] = '/';
        cwd[prev_size] = '\0';
    }
    strcat(cwd, files[current_file].name);

    strcpy(path_to_copied_file, cwd);

    copy_file_ptr = fopen(cwd, "r");

    if (copy_file_ptr == NULL) {
        char *buf = "open file error\n";
        perror(buf);
    }
    cwd[prev_size] = '\0';
    cut = 0;
}

void add_(char *buf) {
    size_t prev_size = strlen(buf);
    buf[prev_size] = '_';
    buf[prev_size + 1] = '\0';
}

void paste_file() {
    if (copy_file_ptr == NULL) {
        return;
    }
    size_t prev_size = strlen(cwd);
    if (cwd[prev_size - 1] != '/') {
        cwd[prev_size++] = '/';
        cwd[prev_size] = '\0';
    }

    strcat(cwd, copied_file_name);
    while (1) {
        FILE *cur_file;
        if ((cur_file = fopen(cwd, "r")))
        {
            fclose(cur_file);
            add_(cwd);
        } else {
            break;
        }
    }

    paste_file_ptr = fopen(cwd, "w");
    if (paste_file_ptr == NULL) {
        char *buf = "open file error\n";
        perror(buf);
    }
    cwd[prev_size] = '\0';

    char c;
    c = (char)fgetc(copy_file_ptr);
    while (c != EOF) {
        fputc(c, paste_file_ptr);
        c = (char)fgetc(copy_file_ptr);
    }
    fclose(copy_file_ptr);
    copy_file_ptr = NULL;
    fclose(paste_file_ptr);

    if (cut) {
        if (remove(path_to_copied_file)) {
            char *buf = "can't remove file\n";
            perror(buf);
        }
        cut = 0;
    }

    update();
}

void cut_file() {
    copy_file();
    cut = 1;
}