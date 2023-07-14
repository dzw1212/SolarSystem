#include "Log.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"


std::shared_ptr<spdlog::logger> Log::m_FileLogger = spdlog::basic_logger_mt("File Logger", "./Log/log.txt", true);
std::shared_ptr<spdlog::logger> Log::m_ConsoleLogger = spdlog::stdout_color_mt("Console Logger");


