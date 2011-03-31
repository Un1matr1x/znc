/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "Client.h"
#include "Server.h"

#include <syslog.h>

class CAdminLogMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CAdminLogMod) {
		openlog("znc", LOG_PID, LOG_DAEMON);
	}

	virtual ~CAdminLogMod() {
		Log("Logging ended.");
		closelog();
	}

	virtual bool OnLoad(const CString & sArgs, CString & sMessage) {
		m_eLogMode = LOG_TO_FILE;

		time_t curtime;
		tm* timeinfo;
		char buf[10];

		time(&curtime);
		timeinfo = localtime(&curtime);
		strftime(buf,sizeof(buf),"%Y-%U",timeinfo);

		m_sLogFile = GetSavePath() + "/" + buf + "-znc.log";

		Log("Logging started. ZNC PID[" + CString(getpid()) + "] UID/GID[" + CString(getuid()) + ":" + CString(getgid()) + "]");
		return true;
	}

	virtual void OnIRCConnected() {
		Log("[" + m_pUser->GetUserName() + "] connected to IRC: " + m_pUser->GetCurrentServer()->GetName());
	}

	virtual void OnIRCDisconnected() {
		Log("[" + m_pUser->GetUserName() + "] disconnected from IRC");
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (sLine.Equals("ERROR ", false, 6)) {
			//ERROR :Closing Link: nick[24.24.24.24] (Excess Flood)
			//ERROR :Closing Link: nick[24.24.24.24] Killer (Local kill by Killer (reason))
			CString sError(sLine.substr(6));
			if (sError.Left(1) == ":")
				sError.LeftChomp();
			Log("[" + m_pUser->GetUserName() + "] disconnected from IRC: " +
			    m_pUser->GetCurrentServer()->GetName() + " [" + sError + "]", LOG_NOTICE);
		}
		return CONTINUE;
        }

	virtual void OnClientLogin() {
		Log("[" + m_pUser->GetUserName() + "] connected to ZNC from " + m_pClient->GetRemoteIP());
	}

	virtual void OnClientDisconnect() {
		Log("[" + m_pUser->GetUserName() + "] disconnected from ZNC from " + m_pClient->GetRemoteIP());
	}

	virtual void OnFailedLogin(const CString& sUsername, const CString& sRemoteIP) {
		Log("[" + sUsername + "] failed to login from " + sRemoteIP, LOG_WARNING);
	}

	void Log(CString sLine, int iPrio = LOG_INFO) {

		if (m_eLogMode & LOG_TO_FILE) {
			time_t curtime;
			tm* timeinfo;
			char buf[23];

			time(&curtime);
			timeinfo = localtime(&curtime);
			strftime(buf,sizeof(buf),"[%Y-%m-%d %H:%M:%S] ",timeinfo);

			CFile LogFile(m_sLogFile);

			if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT))
				LogFile.Write(buf + sLine + "\n");
			else
				DEBUG("Failed to write to [" << m_sLogFile  << "]: " << strerror(errno));
		}
	}

	virtual void OnModCommand(const CString& sCommand) {
		if (!GetUser()->IsAdmin()) {
			PutModule("Access denied");
			return;
		}

		CString sCmd = sCommand.Token(0);

		if (sCmd.Equals("target")) {
			CString sArg = sCommand.Token(1, true);
			CString sTarget;
			CString sMessage;
			LogMode mode;

			if (sArg.Equals("file")) {
				sTarget = "file";
				sMessage = "Now only logging to file";
				mode = LOG_TO_FILE;
			} else {
				PutModule("Unknown target");
				return;
			}

			Log(sMessage);
			SetNV("target", sTarget);
			m_eLogMode = mode;
			PutModule(sMessage);
		} else if (sCmd.Equals("show")) {
			CString sTarget;

			switch (m_eLogMode)
			{
			case LOG_TO_FILE:
				sTarget = "file";
				break;
			}

			PutModule("Logging is enabled for " + sTarget);
			PutModule("Log file will be written to [" + m_sLogFile + "]");
		} else
			PutModule("Commands: show");
	}
private:
	enum LogMode {
		LOG_TO_FILE   = 1 << 0,
	};
	LogMode m_eLogMode;
	CString m_sLogFile;
};

GLOBALMODULEDEFS(CAdminLogMod, "Log ZNC events to a weekly rotating file.")
