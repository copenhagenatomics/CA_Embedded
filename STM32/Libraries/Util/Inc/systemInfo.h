#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

/* Generic info about PCB and git build system/data.
 * @return info about system in null terminated string. */
const char* systemInfo(const char* productType, const char* mcuFamily, const char* pcbVersion);

#endif // SYSTEM_INFO_H
