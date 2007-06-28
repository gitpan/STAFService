/*****************************************************************************/
/* Software Testing Automation Framework (STAF)                              */
/* (C) Copyright IBM Corp. 2004                                              */
/*                                                                           */
/* This software is licensed under the Common Public License (CPL) V1.0.     */
/*****************************************************************************/

#ifndef STAF_PerlService
#define STAF_PerlService

enum PerlServiceIPCMessageType_t
{
    PERL_SERVICE_PERL_INTERPRETER_PING        = 0,
    PERL_SERVICE_LOAD                         = 1,
    PERL_SERVICE_INIT                         = 2,
    PERL_SERVICE_ACCEPT_REQUEST               = 3,
    PERL_SERVICE_TERM                         = 4,
    PERL_SERVICE_DESTRUCT                     = 5,
    PERL_SERVICE_PERL_INTERPRETER_EXIT        = 6,
    PERL_SERVICE_PERL_INTERPRETER_FIN         = 7
};

#endif
