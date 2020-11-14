//
// Created by wjx on 2020/11/12.
//

#include <gtest/gtest.h>
#include <pqos.h>
#include <cstdlib>

extern "C" {
#include "../control_scheme.h"
}


class ControlSchemeTest : public ::testing::Test {
protected:
    struct pqos_config config{};

    void SetUp() override {
        memset(&config, 0, sizeof(struct pqos_config));
        config.interface = PQOS_INTER_OS;
        config.fd_log = STDOUT_FILENO;
        pqos_init(&config);
    }

    void TearDown() override {
        pqos_fini();
    }
};

TEST_F(ControlSchemeTest, getInfo) {
    CLOSCapabilityInfo info{};
    ASSERT_EQ(0, controlSchemeGetInfo(&info));
    ASSERT_NE(0, info.numCatClos);
    ASSERT_NE(0, info.numMbaClos);
    ASSERT_NE(0, info.minLLCWays);
    ASSERT_NE(0, info.maxLLCWays);
}

TEST_F(ControlSchemeTest, setNormalScheme) {
    CLOSCapabilityInfo info{};
    controlSchemeGetInfo(&info);
    int closSize = 3;
    unsigned int numCLos = (info.numCatClos > info.numMbaClos ? info.numMbaClos : info.numCatClos) - 1;
    unsigned int numPid = closSize * numCLos;
    auto *list = (pid_t *) malloc(sizeof(pid_t) * numPid);
    pid_t curr = 1;
    for (int i = 0; i < numPid; i++) {
        while (-1 == kill(curr, 0)) {
            curr++;
        }
        list[i] = curr;
        curr++;
    }

    auto *schemes = (CLOSScheme *) malloc(sizeof(CLOSScheme) * numCLos);

    for (int i = 0; i < numCLos; i++) {
        schemes[i].closNum = i + 1;
        schemes[i].processList = list + i * 3;
        schemes[i].lenProcessList = 3;
        schemes[i].mbaThrottle = 100;
        schemes[i].llc = 0x7ff;
    }

    ASSERT_EQ(0, controlSchemeSet(schemes, numCLos));

    unsigned int buf;
    for (int i = 0; i < numCLos; i++) {
        for (int j = 0; j < closSize; j++) {
            ASSERT_EQ(0, pqos_alloc_assoc_get_pid(list[i * closSize + j], &buf));
            ASSERT_EQ(i + 1, buf);
        }
    }

    // 复原
    for (int i = 0; i < numPid; i++) {
        pqos_alloc_assoc_set_pid(list[i], 0);
    }
    free(list);
    free(schemes);
}

TEST_F(ControlSchemeTest, setTooManyScheme) {
    CLOSCapabilityInfo info{};
    controlSchemeGetInfo(&info);
    unsigned int minClos = info.numCatClos < info.numMbaClos ? info.numCatClos : info.numMbaClos;
    CLOSScheme *schemes = (CLOSScheme *) malloc((minClos + 1) * sizeof(CLOSScheme));
    pid_t dummy = 1;
    for (int i = 0; i < minClos; i++) {
        schemes[i].closNum = i + 1;
        schemes[i].mbaThrottle = 100;
        schemes[i].processList = &dummy;
        schemes[i].lenProcessList = 1;
        schemes[i].llc = 0xf;
    }
    ASSERT_NE(0, controlSchemeSet(schemes, minClos));
    free(schemes);
}