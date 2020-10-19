#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <map>

namespace xre
{
	enum LOG_LEVEL
	{
		FATAL = 4,
		ERROR = 3,
		WARN = 2,
		INFO = 1,
		DEBUG = 0
	};

	enum LOG_LEVEL_FILTER_TYPE
	{
		EQUAL,
		GREATER_OR_EQUAL
	};

	class LogModule
	{
	private:
		static LogModule* logger_instance;

		int m_log_level;
		int m_log_level_filter;
		std::map<int, std::string> m_log_level_mappings;
		std::map<std::string, int> m_log_string_count;
		int max_log_string_occurs;

		LogModule();
		void printLogMessage(const std::string& m1, const std::string& m2, const std::string& m3);

	public:
		static LogModule* getLoggerInstance();
		LogModule(LogModule& other) = delete;

		void log(const LOG_LEVEL& log_level, const std::string& source, const std::string& message);
		void setLogLevel(const LOG_LEVEL& log_level, const LOG_LEVEL_FILTER_TYPE& log_level_filter);
	};
}

#endif