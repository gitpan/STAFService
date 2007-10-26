
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
	char *moduleName;
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
		STAFStringFreeBuffer(*tmpString, NULL);
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

void SetErrorBuffer(STAFString_t *pErrorBuffer, const char *err_str) {
	STAFStringConstruct(pErrorBuffer, err_str, strlen(err_str), NULL);
}

/** my_eval_sv(code)
 ** kinda like eval_sv(), 
 ** but we pop the return value off the stack 
 **/
int my_eval_sv(pTHX_ SV *sv) {
    dSP;
    SV *retval;
    int ret_int;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
    eval_sv(sv, G_SCALAR);     
	SPAGAIN;
    retval = POPs;
    PUTBACK;     
	if (SvTRUE(ERRSV)) {
 		ret_int = 0;
		fprintf(stderr, "Perl Error: %s\n", SvPVX(ERRSV));
	} else {
		ret_int = 1;
	}
    FREETMPS;
    LEAVE;
	return ret_int;
}

STAFRC_t RedirectPerlStdout(void *holder, STAFString_t WriteLocation, STAFString_t ServiceName, unsigned int maxlogs, long maxlogsize, STAFString_t *pErrorBuffer) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	const char *command_fmt = 
		"{; "
		  "my $write_location = '%s';\n"
		  "my $service_name = '%s';\n"
		  "my $maxlogsize = %d;\n"
		  "my $maxlogs = %d;\n"
		  "my $service_module = '%s';\n"
		  "\n"
		  "my $dir = $write_location.'/lang';\n"
		  "mkdir $dir unless -d $dir;\n"
		  "$dir .= '/perl';\n"
		  "mkdir $dir unless -d $dir;\n"
		  "$dir .= '/'.$service_name;\n"
		  "mkdir $dir unless -d $dir;\n"
		  "if ((-e $dir.'/PerlInterpreterLog.1') && (-s $dir.'/PerlInterpreterLog.1' > $maxlogsize)) {\n"
		    "unlink $dir.'/PerlInterpreterLog.'.$maxlogs if -e $dir.'/PerlInterpreterLog.'.$maxlogs;\n"
		    "for (my $ix=$maxlogs-1; $ix>0; $ix++) { \n"
			  "rename($dir.'/PerlInterpreterLog.'.$ix, $dir.'/PerlInterpreterLog.'.($ix+1)) }}\n"
//		  "open my $fh, \">>\", $dir.'/PerlInterpreterLog.1' or die 'Failed to redirect';\n"
//		  "my $old_fh = select $fh;\n"
		  "open $STAFSERVICE::_REDIRECT_HANDLE, \">>\", $dir.'/PerlInterpreterLog.1' or die 'Failed to redirect';\n"
		  "select $STAFSERVICE::_REDIRECT_HANDLE;\n"
		  "print '*' x 80, \"\\n\";\n"
		  "print '*** ', scalar(localtime), ' - Start of Log for PerlServiceName: ', $service_name, \"\\n\";\n" 
		  "print '*** PerlService Executable: ', $service_module, \"\\n\";\n"
		  "$|=1;\n"
		"}\n";
	char *write_location = NULL;
	char *service_name = NULL;
	SV *command = newSVpvf(command_fmt, toChar(WriteLocation, &write_location), toChar(ServiceName, &service_name), 
							maxlogsize, maxlogs, ph->moduleName);
	//fprintf(stderr, "The redirection command: |%s|\n", SvPVX(command));
	toChar(NULL, &write_location);
	toChar(NULL, &service_name);
	int ret = my_eval_sv(aTHX_ command);
	SvREFCNT_dec(command);
	if (ret == 0) {
		SetErrorBuffer(pErrorBuffer, "Error: Redirection failed!");
		fprintf(stderr, "Error: Redirection failed!");
		return kSTAFUnknownError;
	}
	return kSTAFOk;
}

void my_load_module(pTHX_ const char *module_name) {
	SV *sv_name = newSVpv(module_name, 0);
	load_module(PERL_LOADMOD_NOIMPORT, sv_name, Nullsv);
}

STAFRC_t PreparePerlInterpreter(void *holder, STAFString_t library_name, STAFString_t *pErrorBuffer) {
	char *acsii_name;
	unsigned int i, len, rc;
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);

	toChar(library_name, &acsii_name);
	len = strlen(acsii_name);
	for (i=0; i<len; i++) {
		char c = acsii_name[i];
		if (! ( ( c >= 'a' && c <= 'z' ) ||
				( c >= 'A' && c <= 'Z' ) ||
				( c >= '0' && c <= '9' ) ||
				( c == '_' || c == ':')
			  )) {
			toChar(NULL, &acsii_name);
			SetErrorBuffer(pErrorBuffer, "Illigal name"); // FIXME: this string arrive nowhere!
			fprintf(stderr, "Illigal name");
			return kSTAFUnknownError;
		}
	}
	
	SV *command = newSVpvf("require %s", acsii_name);
	toChar(NULL, &acsii_name);
    dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
    eval_sv(command, G_EVAL | G_DISCARD);
	SvREFCNT_dec(command);
	
	if (SvTRUE(ERRSV)) {
		SetErrorBuffer(pErrorBuffer, SvPVX(ERRSV)); // FIXME: this string arrive nowhere!
		fprintf(stderr, "Error: %s", SvPVX(ERRSV));
		rc = kSTAFUnknownError;
	} else {
		rc = kSTAFOk;
	}
    FREETMPS;
    LEAVE;
	return rc;
}

void storePV2HV(pTHX_ HV *hv, const char *key, const char *value) {
	hv_store(hv, key, strlen(key), newSVpv(value, 0), 0);
}

void storeIV2HV(pTHX_ HV *hv, const char *key, int value) {
	hv_store(hv, key, strlen(key), newSViv(value), 0);
}

void PopulatePerlHolder(void *holder, STAFString_t service_name, STAFString_t library_name, STAFServiceType_t serviceType) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);

	char *tmp = NULL;
	ph->object = NULL;
	ph->data = newHV();
	storePV2HV(aTHX_ ph->data, "ServiceName", toChar(service_name, &tmp));
	storeIV2HV(aTHX_ ph->data, "ServiceType", serviceType);

	toChar(library_name, &tmp);
	int len = strlen(tmp);
	ph->moduleName = (char *)malloc(len+1);
	strcpy(ph->moduleName, tmp);
	toChar(NULL, &tmp);
}

void *CreatePerl() {
	InitPerlEnviroment();
    char *embedding[] = { "", "-e", "0" };

	PHolder *ph = (PHolder*)malloc(sizeof(PHolder));
	if (ph==NULL) return NULL;

    PerlInterpreter *pperl = perl_alloc();
	if (pperl==NULL) return NULL;

    perl_construct( pperl );
	dTHXa(pperl);
	perl_parse(pperl, xs_init, 3, embedding, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    perl_run(pperl);

	my_load_module(aTHX_ "lib");

	ph->perl = pperl;
	return ph;
}

void perl_uselib(void *holder, STAFString_t path) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	char *tmp = NULL;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVpv("lib", 0)));
	XPUSHs(sv_2mortal(newSVpv(toChar(path, &tmp), 0)));
	PUTBACK;
    call_method("import", G_DISCARD | G_EVAL);

    FREETMPS;
    LEAVE;
	toChar(NULL, &tmp);
}

SV *call_new(pTHX_ char *module_name, HV *hv, STAFString_t *pErrorBuffer) {
	SV *ret = NULL;
	int count;
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVpv(module_name, 0)));
	XPUSHs(sv_2mortal(newRV_inc((SV*)hv)));
	PUTBACK;
    count = call_method("new", G_SCALAR | G_EVAL);
    SPAGAIN;
	ret = POPs;
	if (SvTRUE(ERRSV)) {
		// There was an error
		SetErrorBuffer(pErrorBuffer, SvPVX(ERRSV));
		ret = NULL;
    } else {
		if (!SvOK(ret)) {
			// undefined result?!
			SetErrorBuffer(pErrorBuffer, "Unexpected Result Returned!");
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
	SV *object = call_new(aTHX_ ph->moduleName, ph->data, pErrorBuffer);
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

STAFRC_t Terminate(void *holder) {
	PHolder *ph = (PHolder *)holder;
	dTHXa(ph->perl);
	PERL_SET_CONTEXT(ph->perl);
	if (ph->data!=NULL) {
		SvREFCNT_dec(ph->data);
		ph->data = NULL;
	}
	if (ph->object!=NULL) {
		SvREFCNT_dec(ph->object);
		ph->object = NULL;
	}
	return kSTAFOk;
}

STAFRC_t DestroyPerl(void *holder) {
	PHolder *ph = (PHolder *)holder;
	PerlInterpreter *pperl = ph->perl;
	free(ph->moduleName);
	free(ph);
	dTHXa(pperl);
	PERL_SET_CONTEXT(pperl);
	perl_destruct(pperl);
    perl_free(pperl);
	DestroyPerlEnviroment();
	return kSTAFOk;
}
