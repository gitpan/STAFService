/*****************************************************************************/
/* Software Testing Automation Framework (STAF)                              */
/* (C) Copyright IBM Corp. 2004                                              */
/*                                                                           */
/* This software is licensed under the Common Public License (CPL) V1.0.     */
/*****************************************************************************/

#include "STAF.h"
#include <vector>
#include <map>
#include "STAFServiceInterface.h"
#include "STAFPerlService.h"
#include "STAFUtil.h"
#include "STAFThread.h"
#include "STAFEventSem.h"
#include "STAFMutexSem.h"
#include "STAFProcess.h"
#include "STAFFileSystem.h"
#include "STAFTrace.h"
#include "STAF_fstream.h"
#include "perlglue.h"

static STAFString sLocal("local");
static STAFString sIPCName("IPCNAME");

typedef struct STAFProcPerlServiceDataST {
	void *perl;
	STAFMutexSem mutex;
} STAFProcPerlServiceData;

STAFRC_t ParseParameters(STAFServiceInfoLevel30 *pInfo, unsigned int *maxLogs, long *maxLogSize, STAFString_t *useLib, STAFString_t *pErrorBuffer);
STAFRC_t CreateLogFile(STAFServiceInfoLevel30 *pInfo, STAFString_t *log_file_name, unsigned int maxLogs, long maxLogSize, STAFString_t *pErrorBuffer);

STAFRC_t STAFServiceConstruct(STAFServiceHandle_t *pServiceHandle,
                              void *pServiceInfo, unsigned int infoLevel,
                              STAFString_t *pErrorBuffer)
{
    try
    {
        if (infoLevel != 30) return kSTAFInvalidAPILevel;

        STAFServiceInfoLevel30 *pInfo = static_cast<STAFServiceInfoLevel30 *>(pServiceInfo);
        STAFString perlServiceName = pInfo->name;
        STAFString perlServiceExec = pInfo->exec;
        unsigned int maxLogs = 5; // Defaults to keeping 5 PerlInterpreter logs
        long maxLogSize = 1048576; // Default size of each log is 1M
		STAFString_t useLib = NULL;
		STAFRC_t ret;

        // Walk through and verify the config options
		ret = ParseParameters(pInfo, &maxLogs, &maxLogSize, &useLib, pErrorBuffer);
		if (ret!=kSTAFOk) return ret;
		if (useLib!=NULL) {
			STAFString tmp = useLib;
			STAFStringAssign(useLib, tmp.replace("\\", "/").getImpl(), NULL);
		}

		STAFString_t logFilename;
		ret = CreateLogFile(pInfo, &logFilename, maxLogs, maxLogSize, pErrorBuffer);
		if (ret!=kSTAFOk) return ret;

		STAFProcPerlServiceData *pData = new STAFProcPerlServiceData;
		if (pData==NULL) return kSTAFUnknownError;

		pData->perl = CreatePerl(pInfo->name, pInfo->exec, pInfo->serviceType, logFilename, useLib);
		if (pData->perl==NULL) {
			free(pData);
			return kSTAFUnknownError;
		}

		*pServiceHandle=pData;
        return kSTAFOk;
    }
    catch (STAFException &e)
    {
        e.trace("PLSTAF.STAFServiceConstruct()"); // Should do something here
    }
    catch (...)
    {
        STAFTrace::trace(
            kSTAFTraceError,
            "Caught unknown exception in PLSTAF.STAFServiceConstruct()");
    }

    return kSTAFUnknownError;
}

STAFRC_t STAFServiceDestruct(STAFServiceHandle_t *serviceHandle,
                             void *pDestructInfo,
                             unsigned int destructLevel,
                             STAFString_t *pErrorBuffer)
{
    try
    {
        if (destructLevel != 0) return kSTAFInvalidAPILevel;

        STAFProcPerlServiceData *pData =
            static_cast<STAFProcPerlServiceData *>(*serviceHandle);

		DestroyPerl(pData->perl);

        delete pData;
        *serviceHandle = 0;

        return kSTAFOk;
    }
    catch (STAFException &e)
    {
        e.trace("PLSTAF.STAFServiceDestruct()");
    }
    catch (...)
    {
        STAFTrace::trace(
            kSTAFTraceError,
            "Caught unknown exception in PLSTAF.STAFServiceDestruct()");
    }

    return kSTAFUnknownError;
}


STAFRC_t STAFServiceInit(STAFServiceHandle_t serviceHandle,
                         void *pInitInfo, unsigned int initLevel,
                         STAFString_t *pErrorBuffer)
{
    try
    {
        if (initLevel != 30) return kSTAFInvalidAPILevel;

        STAFProcPerlServiceData *pData =
            static_cast<STAFProcPerlServiceData *>(serviceHandle);
        STAFServiceInitLevel30 *pInfo =
            static_cast<STAFServiceInitLevel30 *>(pInitInfo);

		STAFMutexSemLock lock(pData->mutex);

		return InitService(pData->perl, pInfo->parms, pInfo->writeLocation, pErrorBuffer);
    }
    catch (STAFException &e)
    {
        e.trace("PLSTAF.STAFServiceInit()");
    }
    catch (...)
    {
        STAFTrace::trace(
            kSTAFTraceError,
            "Caught unknown exception in PLSTAF.STAFServiceInit()");
    }

    return kSTAFUnknownError;
}

STAFRC_t STAFServiceTerm(STAFServiceHandle_t serviceHandle,
                         void *pTermInfo, unsigned int termLevel,
                         STAFString_t *pErrorBuffer)
{
    try
    {
        if (termLevel != 0) return kSTAFInvalidAPILevel;

        STAFProcPerlServiceData *pData =
            static_cast<STAFProcPerlServiceData *>(serviceHandle);

		STAFMutexSemLock lock(pData->mutex);

		return Terminate(pData->perl, pErrorBuffer);
    }
    catch (STAFException &e)
    {
        e.trace("PLSTAF.STAFServiceTerm()");
    }
    catch (...)
    {
        STAFTrace::trace(
            kSTAFTraceError,
            "Caught unknown exception in PLSTAF.STAFServiceTerm()");
    }

    return kSTAFUnknownError;
}


STAFRC_t STAFServiceAcceptRequest(STAFServiceHandle_t serviceHandle,
                                  void *pRequestInfo, unsigned int reqLevel,
                                  STAFString_t *pResultBuffer)
{
    try
    {
        if (reqLevel != 30) return kSTAFInvalidAPILevel;

        STAFProcPerlServiceData *pData =
            static_cast<STAFProcPerlServiceData *>(serviceHandle);
        STAFServiceRequestLevel30 *pInfo =
            static_cast<STAFServiceRequestLevel30 *>(pRequestInfo);
		
		STAFMutexSemLock lock(pData->mutex);

		return ServeRequest(pData->perl, pInfo, pResultBuffer);

    }
    catch (STAFException &e)
    {
        e.trace("PLSTAF.STAFServiceAcceptRequest");
    }
    catch (...)
    {
        STAFTrace::trace(
            kSTAFTraceError,
            "Caught unknown exception in PLSTAF.STAFServiceAcceptRequest()");
    }

    *pResultBuffer = STAFString().adoptImpl();

    return kSTAFUnknownError;
}


STAFRC_t STAFServiceGetLevelBounds(unsigned int levelID,
                                   unsigned int *minimum,
                                   unsigned int *maximum)
{
    switch (levelID)
    {
        case kServiceInfo:
        {
            *minimum = 30;
            *maximum = 30;
            break;
        }
        case kServiceInit:
        {
            *minimum = 30;
            *maximum = 30;
            break;
        }
        case kServiceAcceptRequest:
        {
            *minimum = 30;
            *maximum = 30;
            break;
        }
        case kServiceTerm:
        case kServiceDestruct:
        {
            *minimum = 0;
            *maximum = 0;
            break;
        }
        default:
        {
            return kSTAFInvalidAPILevel;
        }
    }

    return kSTAFOk;
}

// Helper Functions

STAFRC_t ParseParameters(STAFServiceInfoLevel30 *pInfo, unsigned int *maxLogs, long *maxLogSize, STAFString_t *useLib, STAFString_t *pErrorBuffer) {

        for (unsigned int i = 0; i < pInfo->numOptions; ++i)
        {
            STAFString upperOptionName =
                       STAFString(pInfo->pOptionName[i]).upperCase();
            STAFString optionValue(pInfo->pOptionValue[i]);

            if (upperOptionName == "MAXLOGS")
            {
                // Check to make sure it is an integer value > 0

                STAFString maxLogsStr = optionValue;

                if (maxLogsStr.isDigits() && (maxLogsStr.asUInt() > 0))
                {
                    *maxLogs = maxLogsStr.asUInt();
                }
                else
                {
					STAFString perlServiceName(pInfo->name);
					STAFString perlServiceExec(pInfo->exec);
                    STAFString errmsg(
                        "Error constructing the Perl Interpreter using "
                        "Perl Service Name: " + perlServiceName +
                        ", PerlServiceExec " + perlServiceExec +
                        ", RC: " + STAFString(kSTAFInvalidValue) +
                        ", MAXLOGS must be an positive integer");
                    *pErrorBuffer = errmsg.adoptImpl();

                    return kSTAFServiceConfigurationError;
                }
            }
            else if (upperOptionName == "MAXLOGSIZE")
            {
                // Check to make sure it is an integer value > 0

                STAFString maxLogSizeStr = optionValue;

                if (maxLogSizeStr.isDigits() && (maxLogSizeStr.asUInt() > 0))
                {
                    *maxLogSize = (long)maxLogSizeStr.asUInt();
                }
                else
                {
					STAFString perlServiceName(pInfo->name);
					STAFString perlServiceExec(pInfo->exec);
                    STAFString errmsg(
                        "Error constructing the Perl Interpreter using "
                        "Perl Service Name: " + perlServiceName +
                        ", PerlServiceExec " + perlServiceExec +
                        ", RC: " + STAFString(kSTAFInvalidValue) +
                        ", MAXLOGSIZE must be an positive integer");
                    *pErrorBuffer = errmsg.adoptImpl();

                    return kSTAFServiceConfigurationError;
                }
			}
            else if (upperOptionName == "USELIB")
            {
				if (*useLib==NULL) {
					STAFStringConstructCopy(useLib, STAFString("'").getImpl(), NULL);
					STAFStringConcatenate(*useLib, optionValue.getImpl(), NULL);
					STAFStringConcatenate(*useLib, STAFString("'").getImpl(), NULL);
				} else {
					STAFStringConcatenate(*useLib, STAFString(", '").getImpl(), NULL);
					STAFStringConcatenate(*useLib, optionValue.getImpl(), NULL);
					STAFStringConcatenate(*useLib, STAFString("'").getImpl(), NULL);
				}
            }
            else
            {
                STAFString optionError(pInfo->pOptionName[i]);
                *pErrorBuffer = optionError.adoptImpl();

                return kSTAFServiceConfigurationError;
            }
		}
        return kSTAFOk;
}

STAFRC_t CreateLogFile(STAFServiceInfoLevel30 *pInfo, STAFString_t *log_file_name, unsigned int maxLogs, long maxLogSize, STAFString_t *pErrorBuffer) {
	STAFString perlServiceName(pInfo->name);
	STAFString perlServiceExec(pInfo->exec);
            STAFFSPath logPath;
            logPath.setRoot(pInfo->writeLocation);
            logPath.addDir("lang");
            logPath.addDir("perl");
            logPath.addDir(perlServiceName);

            if (!logPath.exists())
            {
                try
                {
                    // Don't want exceptions here
                    STAFFSEntryPtr dir =
                        logPath.createDirectory(kSTAFFSCreatePath);
                }
                catch (...)
                { /* Do Nothing */ }

                if (!logPath.exists())
                {
                    STAFString errmsg(
                        "Error constructing the Perl Interpreter using "
                        "Perl Service Name: " + perlServiceName +
                        ", PerlServiceExec " + perlServiceExec +
                        ", Error creating PerlInterpreterLog directory: " +
                        logPath.asString());
                    *pErrorBuffer = errmsg.adoptImpl();

                    return kSTAFServiceConfigurationError;
                }
            }

            // Name the current PerlInterpreter log file - PerlInterpreterLog.1

            logPath.setName("PerlInterpreterLog");
            logPath.setExtension("1");
            STAFString logName = logPath.asString();

            // Instead of replacing the PerlInterpreter log file each time a
            // PerlInterpreter is created, use the following PerlInterpreter
            // OPTIONs to determine when to create a new PerlInterpreter log
            // file and how many PerlInterpreter log files to save:
            // - MaxLogs   : Maximum number of PerlInterpreter log files to keep
            // - MaxLogSize: Maximum size of a PerlInterpreter log file in bytes

            // Open the PerlInterpreter log file

            fstream outfile(logName.toCurrentCodePage()->buffer(),
                            ios::out | ios::app);

            if (!outfile)
            {
                STAFString errmsg(
                    "Error constructing the Perl Interpreter using "
                    "Perl Service Name: " + perlServiceName +
                    ", PerlServiceExec " + perlServiceExec +
                    ", RC: " + STAFString(kSTAFFileOpenError) +
                    ", Error opening file " + logName);
                *pErrorBuffer = errmsg.adoptImpl();

                return kSTAFServiceConfigurationError;
            }

            // Figure out how big the PerlInterpreter log file is

            outfile.seekp(0, ios::end);
            unsigned int fileLength = (unsigned int)outfile.tellp();

            if (fileLength > maxLogSize)
            {
                // Roll any existing log files (e.g. Rename
                // PerlInterpreterLog.2.out to PerlInterpreterLog.3,
                // PerlInterpreterLog.1 to PerlInterpreterLog.2, etc) and
                // create a new PerlInterpreterLog.1 file.  If the # of
                // existing logs > MAXLOGS, don't save the oldest log.

                outfile.close();

                STAFFSPath fromLogPath(logPath);

                for (int i = maxLogs; i > 0; --i)
                {
                    fromLogPath.setExtension(STAFString(i));

                    if (fromLogPath.exists() && i < maxLogs)
                    {
                        // Rename PerlInterpreterLog.<i> to
                        // PerlInterpreterLog.<i+1>

                        STAFFSPath toLogPath(fromLogPath);
                        toLogPath.setExtension(STAFString(i + 1));

                        fromLogPath.getEntry()->move(toLogPath.asString());
                    }
                }

                // Open a new empty current log file

                outfile.open(logName.toCurrentCodePage()->buffer(),
                             ios::out | ios::trunc);

                if (!outfile)
                {
                    STAFString errmsg(
                        "Error constructing the Perl Interpreter using "
                        "Perl Service Name: " + perlServiceName +
                        ", PerlServiceExec " + perlServiceExec +
                        ", RC: " + STAFString(kSTAFFileOpenError) +
                        ", Error opening file " + logName);
                    *pErrorBuffer = errmsg.adoptImpl();

                    return kSTAFServiceConfigurationError;
                }
            }

            // Write the PerlInterpreter start information to the
            // PerlInterpreter log file

            STAFString separatorLine("***************************************"
                                     "***************************************");
            STAFString line1("*** " + STAFTimestamp().asString() +
                                  " - Start of Log for PerlServiceName: " +
                                  perlServiceName);
            STAFString line3("*** PerlService Executable: " + perlServiceExec);

            outfile << separatorLine.toCurrentCodePage()->buffer() << endl
                    << line1.toCurrentCodePage()->buffer() << endl
                    << line3.toCurrentCodePage()->buffer() << endl
                    << separatorLine.toCurrentCodePage()->buffer() << endl;

            outfile.close();
		STAFStringConstruct(log_file_name, "", 0, NULL);
		STAFStringAssign(*log_file_name, logName.replace("\\", "/").getImpl(), NULL);
        return kSTAFOk;
}

