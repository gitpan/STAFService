
#ifndef __PERLGLUE_H__
#define __PERLGLUE_H__

#include "STAFServiceInterface.h"

void *CreatePerl(STAFString_t service_name, STAFString_t library_name, STAFServiceType_t serviceType, STAFString_t log_file_name, STAFString_t uselib);
STAFRC_t InitService(void *holder, STAFString_t parms, STAFString_t writeLocation, STAFString_t *pErrorBuffer);
STAFRC_t ServeRequest(void *holder, struct STAFServiceRequestLevel30 *request, STAFString_t *pResultBuffer);
STAFRC_t Terminate(void *holder, STAFString_t *pErrorBuffer);
STAFRC_t DestroyPerl(void *holder);

#endif