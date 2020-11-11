#include <gtest/gtest.h>

extern "C"
{
#include <pqos.h>
#include "../utils/pqos_utils.h"
}

class PqosTest : public ::testing::Test
{
protected:
    struct pqos_config config{};
    struct pqos_cap *cap = nullptr;
    struct pqos_cpuinfo *cpu = nullptr;

    PqosTest()
    {
        memset(&this->config, 0, sizeof(struct pqos_config));
        this->config.fd_log = STDOUT_FILENO;
        this->config.interface = PQOS_INTER_OS;
    }

    void SetUp() override
    {
        pqos_init(&this->config);
        pqos_cap_get((const pqos_cap **)&cap, (const pqos_cpuinfo **)&cpu);
    }

    void TearDown() override
    {
        pqos_fini();
    }
};

TEST_F(PqosTest, getCpuAllCores)
{
    unsigned int count = 0;
    unsigned int *cores = GetAllCoresId(cpu, &count);
    EXPECT_EQ(40, count);
    for (int i = 1; i < count; i++)
    {
        EXPECT_NE(0, cores[i]);
    }
    free(cores);
}