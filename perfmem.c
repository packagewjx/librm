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
#include "log.h"

#define PERF_COUNT "3"
#define PERF_SWITCH_OUTPUT "--switch-output=1s"
#define PERF_OUT_FILE "perf.data"

int lexicographicalSort(const void *s1, const void *s2) {
    return strcmp(s1, s2);
}

extern inline u_int64_t cacheLineAddressOf(u_int64_t addr) {
    return addr & 0xFFFFFFFFFFFFFFC0;
}


int rm_mem_mon_start(pid_t *pidList, int lenPid, struct rm_mem_mon_data *data, const char *requestId) {
    char tmpDir[] = "tmp.XXXXXX";
    mkdtemp(tmpDir);
    char *dir = malloc(11 * sizeof(char));
    data->perfDataDir = memcpy(dir, tmpDir, 11);
    char *pidListString = pidListToCommaSeparatedString(pidList, lenPid);
    char *id = malloc((strlen(requestId) + 1) * sizeof(char));
    strcpy(id, requestId);
    data->groupId = id;

    int pid = fork();
    if (pid == 0) {
        // set follow-fork-mode child
        // set detach-on-fork off
        char outFileName[100];
        pid = getpid();
        sprintf(outFileName, "%s/%s", tmpDir, PERF_OUT_FILE);
        char *args[] = {"perf", "record", "-e", "cpu/mem-loads/p,cpu/mem-stores/p", "-d", "-c", PERF_COUNT, "-o",
                        outFileName,
                        PERF_SWITCH_OUTPUT, "-p", pidListString, 0};
        char *cmd = joinString(args, 12, ' ');
        log_info("启动perf record监控进程组%s，进程号：%d，执行命令：%s", data->groupId, pid, cmd);
        free(cmd);
        execvp(args[0], args);
        // 如果到达下列语句，则说明发生错误
        exit(errno);
    } else if (pid == -1) {
        int err = errno;
        log_error("fork perf record进程失败，错误码为%d", err);
        rmdir(tmpDir);
        free(dir);
        free(id);
        return errno;
    }
    data->perfMemPid = pid;

    // 启动Hit与Miss的监控
    pid = fork();
    if (pid == 0) {
        char outFile[FILENAME_MAX];
        sprintf(outFile, "%s.api.csv", data->groupId);
        char *args[] = {"perf", "stat", "-e",
                        "mem_load_retired.l1_hit,mem_load_retired.l2_hit,mem_load_retired.l3_hit,mem_load_retired.l3_miss,instructions",
                        "-p", pidListString, "-o", outFile, "-x", ",", 0};
        pid = getpid();
        log_info("启动perf stat监控进程组%s，进程号：%d，执行命令: %s\n", data->groupId, pid, joinString(args, 10, ' '));
        execvp(args[0], args);
        exit(errno);
    } else if (pid == -1) {
        int err = errno;
        log_error("fork perf stat进程失败，错误码为%d", err);
        rmdir(tmpDir);
        free(dir);
        free(id);
        kill(data->perfMemPid, SIGTERM);
        return errno;
    }
    data->perfStatPid = pid;

    free(pidListString);
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
        u_int64_t addr;
        sscanf(line, "%*d,%*d,%*i,%lx,%*d,%*i,%*s", &addr);
        result[idx].addr = cacheLineAddressOf(addr);
        idx++;
    }
    log_info("读取到%d条内存访问记录", idx);
    *recordLen = idx;

    int status;
    waitpid(pid, &status, 0);
    if (status != 0) {
        log_error("perf report退出出错，pid %d\n", pid);
    }
    return result;
}

int rm_mem_mon_poll(struct rm_mem_mon_data *data, int *recordLen, struct rm_mem_mon_trace_data **records) {
    if (processRunning(data->perfMemPid)) {
        log_error("perf进程%d不在运行", data->perfMemPid);
        *recordLen = 0;
        *records = NULL;
        return 1;
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
        char perfFileName[PATH_MAX];
        size_t len = sprintf(perfFileName, "%s/%s", data->perfDataDir, ent->d_name);
        char *fileName = malloc(sizeof(char) * len);
        strcpy(fileName, perfFileName);
        fileNames = realloc(fileNames, (lenFileNames + 1) * sizeof(char *));
        fileNames[lenFileNames] = fileName;
        lenFileNames++;
    }
    if (lenFileNames == 0) {
        log_info("perf record 进程 %d 没有获取到监控数据，直接返回", data->perfMemPid);
        *recordLen = 0;
        *records = NULL;
        free(dir);
        return 0;
    }
    // 给文件名排序，确保按时间升序（字典序即可）
    qsort(fileNames, lenFileNames, sizeof(char *), lexicographicalSort);

    int len = 0;
    struct rm_mem_mon_trace_data *result = NULL;
    for (int i = 0; i < lenFileNames; i++) {
        int partLen;
        struct rm_mem_mon_trace_data *partResult = read_perf_data(fileNames[i], &partLen);
        result = realloc(result, (len + partLen) * sizeof(struct rm_mem_mon_trace_data));
        memcpy(result + len, partResult, partLen * sizeof(struct rm_mem_mon_trace_data));
        len += partLen;
        free(partResult);
    }

    *recordLen = len;
    *records = result;

    // 清理资源
    for (int i = 0; i < lenFileNames; i++) {
        if (0 != remove(fileNames[i])) {
            log_error("删除%s文件失败，错误为：%s", fileNames[i], strerror(errno));
        }

        free(fileNames[i]);
    }
    free(fileNames);
    free(dir);

    return 0;
}

int rm_mem_mon_stop(struct rm_mem_mon_data *data) {
    log_info("接收到结束perf监控进程组%s的请求", data->groupId);
    int retVal = kill(data->perfMemPid, SIGTERM);
    if (retVal != 0) {
        int err = errno;
        log_error("结束perf record监控时发送信号到进程%d出错，错误码为%d", data->perfMemPid, err);
    }

    int status;
    if (-1 == waitpid(data->perfMemPid, &status, 0)) {
        int err = errno;
        fprintf(stderr, "等待结束perf record进程 (pid: %d) 错误，错误码为%d", data->perfMemPid, err);
    }

    retVal = kill(data->perfStatPid, SIGINT);
    if (retVal != 0) {
        int err = errno;
        log_error("结束perf stat时发送信号到进程%d出错，错误码为%d", data->perfStatPid, err);
    }
    if (-1 == waitpid(data->perfStatPid, &status, 0)) {
        int err = errno;
        fprintf(stderr, "等待结束perf stat进程 (pid: %d) 错误，错误码为%d", data->perfStatPid, err);
    }

    log_info("成功结束进程组%s的perf监控", data->groupId);

    recursivelyRemove(data->perfDataDir);
    free((void *) data->perfDataDir);
    free((void *) data->groupId);
    return 0;
}

