//
// Created by wjx on 2020/11/12.
//

#include "resource_manager.h"

#include <pqos.h>

int checkScheme(struct rm_clos_scheme *schemes, int lenSchemes) {
    if (schemes == NULL) {
        return ERR_NULL_PTR;
    }
    struct rm_capability_info info;
    int ret = rm_get_capability_info(&info);
    if (ret != PQOS_RETVAL_OK) {
        return ret;
    }

    int lastMba = 0, lastCat = 0;
    for (int i = 0; i < lenSchemes; i++) {
        if (schemes[i].llc > 0) {
            lastCat = lastCat > schemes[i].closNum ? lastCat : schemes[i].closNum;
        }
        if (schemes[i].mbaThrottle > 0) {
            lastMba = lastCat > schemes[i].closNum ? lastCat : schemes[i].closNum;
        }
        if (schemes[i].processList == NULL || schemes[i].lenProcessList == 0) {
            return ERR_EMPTY_PID_LIST;
        }
    }

    if (lastCat + 1 > info.numCatClos) {
        return ERR_TOO_MANY_CAT;
    }
    if (lastMba + 1 > info.numMbaClos) {
        return ERR_TOO_MANY_MBA;
    }

    return PQOS_RETVAL_OK;
}

int rm_control_scheme_set(struct rm_clos_scheme *schemes, int lenSchemes) {
    int ret = checkScheme(schemes, lenSchemes);
    if (ret != PQOS_RETVAL_OK) {
        return ret;
    }

    const struct pqos_cpuinfo *cpu;
    ret = pqos_cap_get(NULL, &cpu);
    if (ret != PQOS_RETVAL_OK) {
        return ret;
    }

    unsigned int mbaIdCount, l3IdCount;
    unsigned int *mbaIds = pqos_cpu_get_mba_ids(cpu, &mbaIdCount);
    unsigned int *l3Ids = pqos_cpu_get_l3cat_ids(cpu, &l3IdCount);
    if (mbaIds == NULL || l3Ids == NULL) {
        return ERR_UNKNOWN;
    }

    for (int i = 0; i < lenSchemes; i++) {
        // 设置L3分配
        if (schemes[i].llc != 0) {
            struct pqos_l3ca l3Ca = {
                    .class_id = schemes[i].closNum,
                    .u = {
                            .ways_mask = schemes[i].llc
                    }
            };
            for (int j = 0; j < l3IdCount; j++) {
                ret = pqos_l3ca_set(l3Ids[j], 1, &l3Ca);
                if (ret != PQOS_RETVAL_OK) {
                    return ret;
                }
            }
        }

        // 设置MBA分配
        if (schemes[i].mbaThrottle != 0) {
            struct pqos_mba mba = {
                    .class_id = schemes[i].closNum,
                    .ctrl = PQOS_MBA_ANY,
                    .mb_max = schemes[i].mbaThrottle
            };
            for (int j = 0; j < mbaIdCount; j++) {
                ret = pqos_mba_set(mbaIds[j], 1, &mba, NULL);
                if (ret != PQOS_RETVAL_OK) {
                    return ret;
                }
            }
        }

        // 设置进程绑定
        for (int j = 0; j < schemes->lenProcessList; j++) {
            // 这里忽略错误。由于可能会有很大量的PID设置，由一个进程设置错误会导致整个过程结束。比如设置过程中pid进程关闭了，重新设置
            // 又有可能新的进程关闭，可能就会多次重试。
            pqos_alloc_assoc_set_pid(schemes[i].processList[j], schemes[i].closNum);
        }
    }

    return 0;
}

int rm_get_capability_info(struct rm_capability_info *info) {
    const struct pqos_cap *cap;
    int ret = pqos_cap_get(&cap, NULL);
    if (ret != PQOS_RETVAL_OK) {
        return ret;
    }

    for (int i = 0; i < cap->num_cap; i++) {
        switch (cap->capabilities[i].type) {
            case PQOS_CAP_TYPE_MBA: {
                info->numMbaClos = cap->capabilities[i].u.mba->num_classes;
                break;
            }

            case PQOS_CAP_TYPE_L3CA: {
                info->maxLLCWays = cap->capabilities[i].u.l3ca->num_ways;
                info->numCatClos = cap->capabilities[i].u.l3ca->num_classes;
                break;
            }
            case PQOS_CAP_TYPE_MON:
            case PQOS_CAP_TYPE_L2CA:
            case PQOS_CAP_TYPE_NUMOF:
                break;
        }
    }

    pqos_l3ca_get_min_cbm_bits(&info->minLLCWays);

    return 0;
}
