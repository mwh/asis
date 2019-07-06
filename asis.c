#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUF_SIZE 1024

char *socket_path = "./socket";

// Report an HTTP error and close the connection.
void error(int cl, int code, char* msg) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.0 %i %s\r\n", code, msg);
    write(cl, buf, strlen(buf));
    write(cl, "\r\n", 2);
    write(cl, buf + 9, strlen(buf) - 9);
    close(cl);
}

// Return 1 if buffer got starts with the contents of buffer want,
// and 0 otherwise.
int expect(char* want, char* got) {
    while (*want != 0) {
        if (*want != *got)
            return 0;
        want++;
        got++;
    }
    return 1;
}

// Serve out the file at path, if it's valid.
// May serve 404 or 500 errors, or any status code
// the .asis file dictates.
//
// Will not serve any file not starting with a
// Status: line.
void serve(int cl, char* path) {
    int fd = open(path, O_RDONLY);
    if (fd <= 0) {
        error(cl, 404, "Not Found");
        return;
    }
    char buf[BUF_SIZE];
    int rc;
    int status = 0;
    while ( (rc = read(fd, buf, BUF_SIZE)) > 0) {
        if (!status) {
            if (!expect("Status: ", buf)) {
                error(cl, 500, "no status line");
                return;
            }
            // Read the numeric status, if valid:
            int i;
            for (i = 8; i < rc; i++) {
                char c = buf[i];
                if (c == '\n') {
                    break;
                }
                if (c == '\r')
                    continue;
                if (c < '0' || c > '9')
                    error(cl, 500, "invalid status code");
                status *= 10;
                status += c - '0';
            }
            if (i == rc) {
                error(cl, 500, "invalid status line");
                return;
            }
            i++; // Skip over terminating newline
            char buf2[BUF_SIZE];
            sprintf(buf2, "HTTP/1.0 %i -\r\n", status);
            write(cl, buf2, strlen(buf2));
            write(cl, buf + i, rc - i);
        } else {
            write(cl, buf, rc);
        }
    }
    close(fd);
    write(cl, "\r\n\r\n", 4);
    close(cl);
}

int main(int argc, char *argv[]) {
    struct sockaddr_un addr;
    char buf[BUF_SIZE];
    int fd, cl, rc;
  
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("Usage: %s [socket_path]\n", argv[0]);
            puts("Serve current directory's .asis files over Unix socket socket_path.");
            puts("If socket_path is not provided, the default is ./socket");
            puts("Options: --help, --version");
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0) {
            printf("%s 0.5\n", argv[0]);
            puts("Copyright (C) 2019 Michael Homer");
            puts("Distributed under the MIT licence.");
            return 0;
        }
        if (argv[1][0] == '-') {
            printf("%s: error: unknown option %s\n", argv[0], argv[1]);
            printf("%s: error: try %s --help\n", argv[0], argv[0]);
            return 1;
        }
        socket_path = argv[1];
    }
    printf("%s: Using socket path %s and awaiting requests\n", argv[0],
            socket_path);
    // Create and bind the socket. Any errors immediately
    // terminate the program. 
    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) {
        perror("socket error");
        exit(-1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind error");
        exit(-1);
    }
    if (listen(fd, 16) < 0) {
        perror("listen error");
        exit(-1);
    }
    // Main loop: await and respond to requests as
    // they come in. Single-threaded and blocking.
    while (1) {
next: // Used for continuing from inner loops.
        if ( (cl = accept(fd, NULL, NULL)) == -1 ) {
            perror("accept error");
            continue;
        }
        char loc[BUF_SIZE];
        loc[0] = 0;
        char prev = 0;
        while ( (rc = read(cl, buf, sizeof(buf))) > 0) {
            // Parse request. loc[0] is 0 at the start,
            // but will have been filled for subsequent
            // chunks.
            if (loc[0] == 0) {
                // This expects the full first line to be included
                // in the first read, and will error out otherwise.
                if (!expect("GET ", buf)) {
                    error(cl, 501, "Request method not implemented");
                    goto next;
                }
                // Skipping over "GET ", read REQUEST_URI
                int i = 4;
                while (buf[i] != ' ' && i < rc) {
                    loc[i - 4] = buf[i++];
                }
                if (i == rc) { // Bail if we reached end of buffer
                    error(cl, 403, "");
                    goto next;
                }
                loc[i - 4] = 0;
            }
            for (int i = 0; i < rc; i++) {
                if (buf[i] == '\n' && prev == '\n') {
                    // Found end of headers - two line breaks in a row
                    goto done;
                }
                if (prev != '\n' || buf[i] != '\r') prev = buf[i];
            }
        }
done:
        if (loc[0] != '/' || strstr(loc, "..") != NULL) {
            error(cl, 403, "invalid location");
        } else {
            struct stat st;
            char name_buf[BUF_SIZE];
            // Look first for foo/index.asis, then just foo.asis, and
            // serve out the first that exists.
            sprintf(name_buf, ".%s/index.asis", loc);
            if (stat(name_buf, &st) == 0) {
                serve(cl, name_buf);
                goto next;
            }
            sprintf(name_buf, "%s.asis", loc);
            if (stat(name_buf, &st) == 0) {
                serve(cl, name_buf);
                goto next;
            }
            // If neither file exists, bail out.
            error(cl, 404, "Not Found");
        }
    }
    return 0;
}

