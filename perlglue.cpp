
#include "perlglue.h"

#define PERL_NO_GET_CONTEXT     /* we want efficiency */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef malloc
#undef free

typedef struct PerlHolder {
	PerlInterpreter *perl;
	HV *data;
	SV *object;
	SV *finish_sub;
} PHolder;

void InitPerlEnviroment() {
	//PERL_SYS_INIT3(&argc,&argv,&env);
}

void DestroyPerlEnviroment() {
	//PERL_SYS_TERM();
}

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void xs_init(pTHX) {
	char *file = __FILE__;
    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

const char *toChar(STAFString_t source, char **tmpString) {
	if (*tmpString!=NULL) {
		free(*tmpString);
		*tmpString = NULL;
	}
	if (source==NULL)
		return NULL;
	unsigned int len;
	STAFRC_t ret;
	ret = STAFStringToCurrentCodePage(source, tmpString, &len, NULL);
	if (ret!=kSTAFOk)
		return NULL;
	return *tmpString;
}

/** my_eval_sv(code)
 ** kinda like eval_sv(), 
 ** but we pop the return value off the stack 
 **/
SV* my_eval_sv(pTHX_ SV *sv) {
    dSP;
    SV* retval;
	PUSHMARK(SP);
    eval_sv(sv, G_SCALAR);     
	SPAGAIN;
    retval = POPs;
    PUTBACK;     
	if (SvTRUE(ERRSV))
 		return NULL;
	SvREFCNT_inc(retval);
	return retval;
}

STAFRC_t RedirectPerlStdout(pTHX_ STAFString_t log_file_name, SV **finish_sub) {
	char *tmp = NULL;
	const char *command_fmt = 
		"{ "
		  "open my $fh, \">>\", \"%s\" or die 'Failed to redirect';"
		  "my $old_fh = select $fh;"
		  "$|=1;"
		  "return sub { "
		     "print 'dis-redirecting\n';"
			 "select $old_fh;"
			 "close $fh; "
			 "};"
		"}";
	SV *command = newSVpvf(command_fmt, toChar(log_file_name, &tmp));
	toChar(NULL, &tmp);
	*finish_sub = my_eval_sv(aTHX_ command);
	SvREFCNT_dec(command);
	if (*finish_sub == NULL) {
		printf("Error: Redirection failed!\n");
		return kSTAFUnknownError;
	}
	return kSTAFOk;
}

void PreparePerlInterpreter(pTHX_ STAFString_t library_name, STAFString_t log_file_name, STAFString_t uselib) {
	char *tmp = NULL;
	SV *command = newSVpv("",0);
	//sv_catpvf(command, " use strict; require Cwd; print \"Hello World, |@INC|$ENV{PERL5LIB}|\".Cwd::getcwd().\"|\\n\";"); 
	if (uselib!=NULL) {
		sv_catpvf(command, " use lib %s;", toChar(uselib, &tmp));
	}
	sv_catpvf(command, " require %s;", toChar(library_name, &tmp));
	eval_sv(command, G_VOID | G_DISCARD);
	SvREFCNT_dec(command);
	toChar(NULL, &tmp);
}

void storePV2HV(pTHX_ HV *hv, const char *key, const char *value) {
	hv_store(hv, key, strlen(key), newSVpv(value, 0), 0);
}

void storeIV2HV(pTHX_ HV *hv, const char *key, int value) {
	hv_store(hv, key, strlen(key), newSViv(value), 0);
}

void PopulatePerlHolder(pTHX_ PHolder *ph, PerlInterpreter *pperl, STAFString_t service_name, STAFString_t library_name, STAFServiceType_t serviceType) {
	ph->perl = pperl;
	char *tmp = NULL;
	ph->object = NULL;
	ph->data = newHV();
	storePV2HV(aTHX_ ph->data, "ServiceName", toChar(service_name, &tmp));
	storePV2HV(aTHX_ ph->data, "Module",      toChar(library_name, &tmp));
	storeIV2HV(aTHX_ ph->data, "ServiceType", serviceType);
	toChar(NULL, &tmp);
}

void *CreatePerl(STAFString_t service_name, STAFString_t library_name, STAFServiceType_t serviceType, STAFString_t log_file_name, STAFString_t uselib) {
	InitPerlEnviroment();
    char *embedding[] = { "", "-e", "0" };

	PHolder *ph = (PHolder*)malloc(sizeof(PHolder));
	if (ph==NULL) {
		return NULL;
	}

    PerlInterpreter *pperl = perl_alloc();
	if (pperl==NULL) {
		free(ph);
		return NULL;
	}
    perl_construct( pperl );
	dTHXa(pperl);
	perl_parse(pperl, xs_init, 3, embedding, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_run(pperl);

	RedirectPerlStdout(aTHX_ log_file_name, &(ph->finish_sub));
	PreparePerlInterpreter(aTHX_ library_name, log_file_name, uselib);
	PopulatePerlHolder(aTHX_ ph, pperl, service_name, library_name, serviceType);

	return (void *)ph;
}

void SetErrorBuffer(STAFString_t *pErrorBuffer, const char *err_str) {
	STAFStringConstruct(pErrorBuffer, err_str, strlen(err_str), NULL);
}

SV *call_new(pTHX_ HV *hv, STAFString_t *pErrorBuffer) {
	const char *key = "Module";
	SV **lib_name = hv_fetch(hv, key, strlen(key), 0);
	if (lib_name==NULL)
		return NULL;
	SV *ret = NULL;
	int count;
	printf("Module name: %s\n", SvPVX(*lib_name));
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
	XPUSHs(*lib_name);
	XPUSHs(sv_2mortal(newRV_inc((SV*)hv)));
	PUTBACK;
    count = call_method("new", G_SCALAR | G_EVAL);
    SPAGAIN;
	if (SvTRUE(ERRSV)) {
		// There was an error
		SetErrorBuffer(pErrorBuffer, SvPVX(ERRSV));
		POPs;
		ret = NULL;
    } else {
		ret = POPs;
		if (!SvOK(ret)) {
			// undefined result?!
			ret = NULL;
		} else if (!(SvROK(ret) && (SvTYPE(ret) & SVt_PVMG))) {
			// Not an object
			SetErrorBuffer(pErrorBuffer, SvPV_nolen(ret));
			ret = NULL;
		} else {
			SvREFCNT_inc(ret);
		}
    }
    FREETMPS;
    LEAVE;
    return ret;
}

STAFRC_t InitService(void *holder, STAFString_t parms, STAFString_t writeLocation, STAFString_t *pErrorBuffer) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	char *tmp = NULL;
	storePV2HV(aTHX_ ph->data, "WriteLocation", toChar(writeLocation, &tmp));
	storePV2HV(aTHX_ ph->data, "Params", toChar(parms, &tmp));
	SV *object = call_new(aTHX_ ph->data, pErrorBuffer);
	toChar(NULL, &tmp);
	if (object==NULL)
		return kSTAFUnknownError;
	ph->object = object;
	return kSTAFOk;
}

STAFRC_t call_accept_request(pTHX_ SV *obj, SV *hash_ref, STAFString_t *pResultBuffer) {
	STAFRC_t ret_code = kSTAFOk;
	int count;
	I32 ax;
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
	XPUSHs(obj);
	XPUSHs(hash_ref);
	PUTBACK;
    count = call_method("AcceptRequest", G_ARRAY | G_EVAL);
    SPAGAIN;
	SP -= count;
    ax = (SP - PL_stack_base) + 1;

	if (SvTRUE(ERRSV)) {
		// There was an error
		SetErrorBuffer(pResultBuffer, SvPVX(ERRSV));
		ret_code = kSTAFUnknownError;
    } else if (count!=2) {
		// The call ended successed but did not return two items!
		SetErrorBuffer(pResultBuffer, "AcceptRequest did not returned two items!");
		ret_code = kSTAFUnknownError;
	} else {
		ret_code = SvIV(ST(0));
		SetErrorBuffer(pResultBuffer, SvPV_nolen(ST(1)));
	}
    FREETMPS;
    LEAVE;
    return ret_code;
}

HV *ConvertRequestStruct(pTHX_ struct STAFServiceRequestLevel30 *request) {
	HV *ret = newHV();
	char *tmp = NULL;
	storePV2HV(aTHX_ ret, "stafInstanceUUID",	toChar(request->stafInstanceUUID, &tmp));
	storePV2HV(aTHX_ ret, "machine",			toChar(request->machine, &tmp));
	storePV2HV(aTHX_ ret, "machineNickname",	toChar(request->machineNickname, &tmp));
	storePV2HV(aTHX_ ret, "handleName",			toChar(request->handleName, &tmp));
	storePV2HV(aTHX_ ret, "request",			toChar(request->request, &tmp));
	storePV2HV(aTHX_ ret, "user",				toChar(request->user, &tmp));
	storePV2HV(aTHX_ ret, "endpoint",			toChar(request->endpoint, &tmp));
	storePV2HV(aTHX_ ret, "physicalInterfaceID",toChar(request->physicalInterfaceID, &tmp));
	storeIV2HV(aTHX_ ret, "trustLevel",			request->trustLevel); 
	storeIV2HV(aTHX_ ret, "isLocalRequest",		request->isLocalRequest); 
	storeIV2HV(aTHX_ ret, "diagEnabled",		request->diagEnabled); 
	storeIV2HV(aTHX_ ret, "trustLevel",			request->trustLevel); 
	storeIV2HV(aTHX_ ret, "requestNumber",		request->requestNumber); 
	storeIV2HV(aTHX_ ret, "handle",				request->handle); // FIXME - should use an object?
	toChar(NULL, &tmp);
	return ret;
}

STAFRC_t ServeRequest(void *holder, struct STAFServiceRequestLevel30 *request, STAFString_t *pResultBuffer) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	HV *params = ConvertRequestStruct(aTHX_ request);
	SV *params_ref = newRV_noinc((SV*)params);
	STAFRC_t ret = call_accept_request(aTHX_ ph->object, params_ref, pResultBuffer);
	SvREFCNT_dec(params_ref);
	return ret;
}

STAFRC_t Terminate(void *holder, STAFString_t *pErrorBuffer) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	if (ph->finish_sub != NULL) {
		eval_sv(ph->finish_sub, G_VOID | G_DISCARD);
		SvREFCNT_dec(ph->finish_sub);
		ph->finish_sub = NULL;
	}
	SvREFCNT_dec(ph->data);
	ph->data = NULL;
	SvREFCNT_dec(ph->object);
	ph->object = NULL;
	return kSTAFOk;
}

STAFRC_t DestroyPerl(void *holder) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	perl_destruct(ph->perl);
    perl_free(ph->perl);
	ph->perl = NULL;
	free(ph);
	DestroyPerlEnviroment();
	return kSTAFOk;
}
