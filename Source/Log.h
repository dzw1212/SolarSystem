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
		m_FileLogger->critical(format, args...);
		m_ConsoleLogger->critical(format, args...);
	}
	
	template<typename... Args>
	static void Error(const std::string& format, Args... args) {
		m_FileLogger->error(format, args...);
		m_ConsoleLogger->error(format, args...);
	}

	template<typename... Args>
	static void Info(const std::string& format, Args... args) {
		m_FileLogger->info(format, args...);
		m_ConsoleLogger->info(format, args...);
	}

	template<typename... Args>
	static void Warn(const std::string& format, Args... args) {
		m_FileLogger->warn(format, args...);
		m_ConsoleLogger->warn(format, args...);
	}

	template<typename... Args>
	static void Trace(const std::string& format, Args... args) {
		m_FileLogger->trace(format, args...);
		m_ConsoleLogger->trace(format, args...);
	}
};