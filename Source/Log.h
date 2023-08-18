#pragma once
#include <memory>
#include "spdlog/spdlog.h"

class Log
{
private:
	Log() {}
	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	static std::shared_ptr<spdlog::logger> m_FileLogger;
	static std::shared_ptr<spdlog::logger> m_ConsoleLogger;
public:
	template<typename... Args>
	static void Critical(const std::string& format, Args... args) {
		std::string strMsg = std::format(format, args...);
		m_FileLogger->critical(strMsg);
		m_ConsoleLogger->critical(strMsg);
	}
	
	template<typename... Args>
	static void Error(const std::string& format, Args... args) {
		std::string strMsg = std::format(format, args...);
		m_FileLogger->error(strMsg);
		m_ConsoleLogger->error(strMsg);
	}

	template<typename... Args>
	static void Info(const std::string& format, Args... args) {
		std::string strMsg = std::format(format, args...);
		m_FileLogger->info(strMsg);
		m_ConsoleLogger->info(strMsg);
	}

	template<typename... Args>
	static void Warn(const std::string& format, Args... args) {
		std::string strMsg = std::format(format, args...);
		m_FileLogger->warn(strMsg);
		m_ConsoleLogger->warn(strMsg);
	}

	template<typename... Args>
	static void Trace(const std::string& format, Args... args) {
		std::string strMsg = std::format(format, args...);
		m_FileLogger->trace(strMsg);
		m_ConsoleLogger->trace(strMsg);
	}
};