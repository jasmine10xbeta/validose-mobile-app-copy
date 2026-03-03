#ifndef SBL_H_
#define SBL_H_

#include <stdbool.h>

void sbl_mbr_configure();
bool sbl_mbr_is_configured();
void sbl_mbr_protect();
void sbl_bootloader_protect();
void sbl_execute();

#endif /* SBL_H_ */
