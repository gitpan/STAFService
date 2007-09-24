
#ifndef __PERLGLUE_H__
#define __PERLGLUE_H__

#include "STAFServiceInterface.h"

typedef struct STAFProcPerlServiceDataST {
	void *perl;
	STAFMutexSem_t mutex;
} STAFProcPerlServiceData;

void *CreatePerl();
void PopulatePerlHolder(void *holder, STAFString_t service_name, STAFString_t library_name, STAFServiceType_t serviceType);
STAFRC_t RedirectPerlStdout(void *holder, STAFString_t WriteLocation, STAFString_t ServiceName, unsigned int maxlogs, long maxlogsize, STAFString_t *pErrorBuffer);
STAFRC_t PreparePerlInterpreter(void *holder, STAFString_t library_name, STAFString_t *pErrorBuffer);
void perl_uselib(void *holder, STAFString_t path);

STAFRC_t InitService(void *holder, STAFString_t parms, STAFString_t writeLocation, STAFString_t *pErrorBuffer);
STAFRC_t ServeRequest(void *holder, struct STAFServiceRequestLevel30 *request, STAFString_t *pResultBuffer);
STAFRC_t Terminate(void *holder);
STAFRC_t DestroyPerl(void *holder);

// Helper functions
unsigned int my_strlen(const char *str);

#endif