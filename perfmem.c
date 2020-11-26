//
// Created by wjx on 2020/11/25.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include "perfmem.h"
#include "utils/general.h"
#include "log/src/log.h"

#define PERF_COUNT "5"
#define PERF_SWITCH_OUTPUT "--switch-output=1s"
#define PERF_OUT_FILE "perf.data"

int lexicographicalSort(const void *s1, const void *s2) {
    return strcmp(s1, s2);
}

int rm_mem_mon_start(pid_t *pidList, int lenPid, struct rm_mem_mon_data *data) {
    char tmpDir[] = "tmp.XXXXXX";
    mkdtemp(tmpDir);
    char *dir = malloc(11 * sizeof(char));
    data->perfDataDir = memcpy(dir, tmpDir, 11);

    int pid = fork();
    if (pid == 0) {
        char *pidListString = pidListToCommaSeparatedString(pidList, lenPid);
        char outFileName[100];
        pid = getpid();
        sprintf(outFileName, "%s/%s", tmpDir, PERF_OUT_FILE);
        char *args[] = {"perf", "record", "-e", "cpu/mem-loads/p,cpu/mem-stores/p", "-d", "-c", PERF_COUNT, "-o",
                        outFileName,
                        PERF_SWITCH_OUTPUT, "--aio=4", "-p", pidListString, 0};
        char *cmd = joinString(args, 13, ' ');
        log_info("启动perf监控，进程号：%d，执行命令：%s", pid, cmd);
        free(cmd);
        execvp(args[0], args);
        // 如果到达下列语句，则说明发生错误
        int err = errno;
        free(pidListString);
        exit(err);
    } else if (pid == -1) {
        int err = errno;
        log_error("fork perf record进程失败，错误码为%d", err);
        rmdir(tmpDir);
        free(dir);
        return errno;
    }

    data->perfPid = pid;
    return 0;
}

struct rm_mem_mon_trace_data *read_perf_data(const char *name, int *recordLen) {
    log_debug("perf report 读取文件%s", name);
    int fd[2];
    pipe(fd);
    int pid = fork();
    if (pid == 0) {
        // 子进程运行perf report并写入到fifo
        close(fd[0]);
        if (fd[1] != STDOUT_FILENO) {
            if (dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO) {
                int err = errno;
                log_error("输出文件%s时，dup出错\n", name);
                exit(err);
            }
        }
        close(fd[1]);
        char *argv[] = {"perf", "mem", "report", "-D", "-x", ",", "-i", (char *) name, "-v", 0};
        char *cmd = joinString(argv, 9, ' ');
        log_info("获取perf数据，执行命令：%s", cmd);
        free(cmd);
        execvp(argv[0], argv);
        exit(0);
    }

    // 父进程处理管道数据
    close(fd[1]);
    FILE *f = fdopen(fd[0], "r");
    const int LineSize = 1024;
    char line[LineSize];
    fgets(line, LineSize, f); //丢弃第一行
    struct rm_mem_mon_trace_data *result = malloc(sizeof(struct rm_mem_mon_trace_data));
    int idx = 0;
    while (fgets(line, LineSize, f) != NULL) {
        line[strcspn(line, "\n")] = 0;
        result = realloc(result, (idx + 1) * sizeof(struct rm_mem_mon_trace_data));
        log_trace("读取到行：%s", line);
        sscanf(line, "%*d,%*d,%*i,%li,%*d,%*i,%*s", &result[idx].addr);
        idx++;
    }
    *recordLen = idx;

    int status;
    waitpid(pid, &status, 0);
    if (status != 0) {
        log_error("perf report退出出错，pid %d\n", pid);
    }
    return result;
}

int rm_mem_mon_poll(struct rm_mem_mon_data *data, int *recordLen, struct rm_mem_mon_trace_data **records) {
    if (kill(data->perfPid, 0) == -1) {
        int err = errno;
        log_error("perf进程%d不存在", data->perfPid);
        return err;
    }

    DIR *dir = opendir(data->perfDataDir);
    struct dirent *ent;

    char **fileNames = NULL;
    int lenFileNames = 0;
    // 获取所有有效的perf.data的文件名
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG) {
            continue;
        }
        // 查看是否由perf.data开头
        int notPerfFile = 0;
        for (int i = 0; i < strlen(PERF_OUT_FILE); i++) {
            if (ent->d_name[i] != PERF_OUT_FILE[i]) {
                notPerfFile = 1;
                break;
            }
        }
        if (notPerfFile) {
            continue;
        }
        // perf.data是perf没写入完成的临时文件，需要跳过
        if (strcmp(PERF_OUT_FILE, ent->d_name) == 0) {
            continue;
        }
        size_t len = strlen(ent->d_name);
        char *fileName = malloc(sizeof(char) * (len + 1));
        strcpy(fileName, ent->d_name);
        fileName[len] = 0;
        fileNames = realloc(fileNames, (lenFileNames + 1) * sizeof(char *));
        fileNames[lenFileNames] = fileName;
        lenFileNames++;
    }
    if (lenFileNames == 0) {
        log_info("perf record 进程 %d 没有获取到监控数据，直接返回", data->perfPid);
        *recordLen = 0;
        *records = NULL;
        return 0;
    }
    // 给文件名排序，确保按时间升序（字典序即可）
    qsort(fileNames, lenFileNames, sizeof(char *), lexicographicalSort);

    int len = 0;
    struct rm_mem_mon_trace_data *result = NULL;
    for (int i = 0; i < lenFileNames; i++) {
        int partLen;
        char perfFileName[100];
        sprintf(perfFileName, "%s/%s", data->perfDataDir, fileNames[i]);
        struct rm_mem_mon_trace_data *partResult = read_perf_data(perfFileName, &partLen);
        result = realloc(result, (len + partLen) * sizeof(struct rm_mem_mon_trace_data));
        memcpy(result + len, partResult, partLen * sizeof(struct rm_mem_mon_trace_data));
        len += partLen;
        free(partResult);
    }

    *recordLen = len;
    *records = result;

    // 清理资源
    for (int i = 0; i < lenFileNames; i++) {
        remove(fileNames[i]);
        free(fileNames[i]);
    }
    free(fileNames);

    return 0;
}

int rm_mem_mon_stop(struct rm_mem_mon_data *data) {
    int retVal = kill(data->perfPid, SIGTERM);
    if (retVal != 0) {
        int err = errno;
        log_error("结束perf监控时发送信号到进程%d出错，错误码为%d", data->perfPid, err);
        return err;
    }
    free((void *) data->perfDataDir);

    int status;
    if (-1 == waitpid(data->perfPid, &status, 0)) {
        int err = errno;
        fprintf(stderr, "结束perf进程 (pid: %d) 错误", data->perfPid);
        return err;
    }
    return 0;
}

