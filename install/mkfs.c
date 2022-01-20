#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char* kernel_processes[] = {
                            "release/grass.elf",
                            "release/echo.elf",
                            //"release/dir.elf",
                            //"release/shell.elf"
};
char buf[1024 * 1024];


int main() {
    freopen("disk.img", "w", stdout);

    /* paging area */
    memset(buf, 0, sizeof(buf));
    write(1, buf, 1024 * 1024);

    /* grass kernel processes */
    int n = sizeof(kernel_processes) / sizeof(char*);
    if (n > 8) {
        fprintf(stderr, "[ERROR] more than 8 kernel processes\n");
        return -1;
    }
    
    fprintf(stderr, "[INFO] Loading %d kernel processes\n", n);
    for (int i = 0; i < n; i++) {
        struct stat st;
        stat(kernel_processes[i], &st);
        fprintf(stderr, "[INFO] Loading %s: %ld bytes\n", kernel_processes[i], st.st_size);

        if (st.st_size > 128 * 1024) {
            fprintf(stderr, "[ERROR] file larger than 128KB\n");
            return -1;
        }

        int size = 0;
        freopen(kernel_processes[i], "r", stdin);
        for (; size < st.st_size; size += read(0, buf, 128 * 1024));
        write(1, buf, st.st_size);
        write(1, buf, 128 * 1024 - st.st_size);
    }

    for (int i = 0; i < 8 - n; i++)
        write(1, buf, 128 * 1024);
        
    /* file system */
    
    fclose(stdout);
    return 0;
}
