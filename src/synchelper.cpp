
#include "synchelper.h"
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int uint;

struct SingleSync_t {
	// the place of this record in the SingleSync's array
    uint request_number;
	// If free, then point to the next free.
	// (or to NULL, if this is the last free record)
	// if in hash, point to the next record in the same bucket
    struct SingleSync_t *next;
	// Event - for waiting and posting the result
    STAFEventSem_t event;
	// The following two parameters are used for passing the results
	// from the worker thread to the requesting thread
	STAFString_t resultBuffer;
	STAFRC_t return_code;
};

struct SyncData_t {
	// Pointer to the first free record. NULL if no record is free.
    SingleSync *first_free;
	// Holds a list of records
    SingleSync **list;
	// how long the list was malloced
    uint list_created;
	// how many items are used in the list
    uint list_occupied;
	// mutex protecting the list. every reading or writing should use this.
    STAFMutexSem_t mutex;
};

void DestroySyncData(SyncData *sd) {
	SingleSync *ss = sd->first_free;
	while (ss != NULL) {
		SingleSync *tmp = ss;
		ss = ss->next;
		free(tmp);
	}
    for (unsigned int ix=0; ix<sd->list_created; ix++) {
        SingleSync *ss = sd->list[ix];
		while (ss != NULL) {
			STAFEventSemDestruct(&(ss->event), NULL);
			SingleSync *tmp = ss;
			ss = ss->next;
			free(tmp);
		}
    }
    free(sd->list);
    STAFMutexSemDestruct(&(sd->mutex), NULL);
    free(sd);
}

void ReleaseSingleSync(SyncData *sd, SingleSync *ss) {
    STAFMutexSemRequest(sd->mutex, -1, NULL);
	uint hashed = ss->request_number % sd->list_created;
	if (sd->list[hashed] == ss) {
		sd->list[hashed] = ss->next;
	} else {
		SingleSync *t = sd->list[hashed];
		while (t!= NULL) {
			if (t->next == ss) {
				t->next = ss->next;
				break;
			}
		}
	}
	if (ss->resultBuffer != NULL) {
		fprintf(stderr, "Warning: STAF::DelayedAnswer() was called for request number %d\n", ss->request_number);
		fprintf(stderr, "   But the data was released without being used.\n");
		fprintf(stderr, "   Please check that you use the currect requestNumber\n");
		fprintf(stderr, "   (and probably now you have a client that will wait forever...)\n");
		STAFStringDestruct(&(ss->resultBuffer) , NULL);
		ss->resultBuffer = NULL;
	}
	ss->request_number = 0;
    ss->next = sd->first_free;
    sd->first_free = ss;
    STAFMutexSemRelease(sd->mutex, NULL);
}

STAFRC_t WaitForSingleSync(SingleSync *ss, STAFString_t *pErrorBuffer) {
    STAFEventSemWait(ss->event, STAF_EVENT_SEM_INDEFINITE_WAIT, NULL);
    STAFEventSemReset(ss->event, NULL);
	*pErrorBuffer = ss->resultBuffer;
	STAFRC_t rc = ss->return_code;
	ss->return_code = 0;
	ss->resultBuffer = NULL;
	return rc;
}

SingleSync *_GetSyncById(SyncData *sd, uint request_number) {
    SingleSync *ss = NULL;
	STAFRC_t ret;
    ret = STAFMutexSemRequest(sd->mutex, -1, NULL);
	if (ret!=kSTAFOk)
		return NULL;
	uint hashed = request_number % sd->list_created;
	ss = sd->list[hashed];
	while (( ss != NULL ) && ( ss->request_number != request_number )) {
		ss = ss->next;
	}
	
    STAFMutexSemRelease(sd->mutex, NULL);
    return ss;
}

void PostSingleSyncByID(SyncData *sd, unsigned int id, STAFRC_t rc, const char *err_str, unsigned int len) {
    SingleSync *ss = _GetSyncById(sd, id);
    if (NULL == ss) {
		fprintf(stderr, "Error: can not find waiting request whose number is %d\n", id);
		fprintf(stderr, "   Please check that you use the currect requestNumber\n");
		fprintf(stderr, "   (and probably now you have a client that will wait forever...)\n");
        return;
	}
	STAFStringConstruct(&(ss->resultBuffer), err_str, len, NULL);
	ss->return_code = rc;
    STAFEventSemPost(ss->event, NULL);
}

SingleSync *GetSingleSync(SyncData *sd, uint request_number) {
    SingleSync *ss = NULL;
	STAFRC_t ret = STAFMutexSemRequest(sd->mutex, -1, NULL);
	if (ret!=kSTAFOk)
		return NULL;
    if (NULL != sd->first_free) {
		uint hashed = request_number % sd->list_created;
        ss = sd->first_free;
        sd->first_free = ss->next;
		ss->next = sd->list[hashed];
		sd->list[hashed] = ss;
		ss->request_number = request_number;
        STAFMutexSemRelease(sd->mutex, NULL);
        return ss;
    }

    if (sd->list_created <= sd->list_occupied) {
        // if there is no place for a new record - need to expend the array
		uint ix;
		uint new_base = sd->list_created * 2;
        SingleSync **list = (SingleSync**)malloc(sizeof(SingleSync*) * new_base);
        if (NULL == list) {
            STAFMutexSemRelease(sd->mutex, NULL);
            return NULL;
        }
		for (ix=0; ix < new_base; ix++) {
			list[ix] = NULL;
		}
        for (ix=0; ix<sd->list_created; ix++) {
			uint hashed;
			SingleSync *tmp, *ss = sd->list[ix];
			while (ss != NULL) {
				tmp = ss->next;
				hashed = request_number % new_base;
				ss->next = list[hashed];
				list[hashed] = ss;
				ss = tmp;
			}
        }
        free(sd->list);
        sd->list = list;
        sd->list_created = sd->list_created * 2;
    }
    
    // now we know that there is enough space for a new record. so lets create it.
    ss = (SingleSync*)malloc(sizeof(SingleSync));
    if (NULL == ss) {
        STAFMutexSemRelease(sd->mutex, NULL);
        return NULL;
    }
    ret = STAFEventSemConstruct(&(ss->event), NULL, NULL);
	if (ret!=kSTAFOk) {
        free(ss);
        STAFMutexSemRelease(sd->mutex, NULL);
		return NULL;
    }
	uint hashed = request_number % sd->list_created;
    ss->request_number = request_number;
    ss->next = sd->list[hashed];
	sd->list_occupied++;
	sd->list[hashed] = ss;
	ss->resultBuffer = NULL;
	ss->return_code = 0;
    
    STAFMutexSemRelease(sd->mutex, NULL);
    return ss;
}

SyncData *CreateSyncData() {
    STAFRC_t ret;
    SyncData *ds = (SyncData*)malloc(sizeof(SyncData));
    if (NULL == ds)
        return NULL;
    ds->list = (SingleSync**)malloc(sizeof(SingleSync*)*10);
    if (NULL == ds->list) {
        free(ds);
        return NULL;
    }
    ds->list_created = 10;
    ds->list_occupied = 0;
    ds->first_free = NULL;
	for (uint ix=0; ix<ds->list_created; ix++) {
		ds->list[ix] = NULL;
	}
    ret = STAFMutexSemConstruct(&(ds->mutex), NULL, NULL);
    if (ret!=kSTAFOk) {
        free(ds->list);
        free(ds);
        return NULL;
    }
    return ds;
}
