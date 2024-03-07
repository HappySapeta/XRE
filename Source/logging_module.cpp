#include <iostream>
#include <logger.h>
#include <map>
#include <tuple>
#include <string>


using namespace xre;

LogModule::LogModule()
	: m_log_level(0), m_log_level_filter(1), max_log_string_occurs(1)
{
	m_log_level_mappings.insert(std::pair<int, std::string>(0, "DEBUG"));
	m_log_level_mappings.insert(std::pair<int, std::string>(1, "INFO"));
	m_log_level_mappings.insert(std::pair<int, std::string>(2, "WARN"));
	m_log_level_mappings.insert(std::pair<int, std::string>(3, "ERROR"));
	m_log_level_mappings.insert(std::pair<int, std::string>(4, "FATAL"));
}

void LogModule::printLogMessage(const std::string& m1, const std::string& m2, const std::string& m3)
{
	std::string log_message = m1 + " :: " + m2 + " :: " + m3;

	int count;
	std::map<std::string, int>::iterator element = m_log_string_count.find(log_message);
	if (element == m_log_string_count.end())
	{
		count = 0;
	}
	else
	{
		count = element->second;
	}

	if (count >= max_log_string_occurs)
	{
		return;
	}
	else
	{
		m_log_string_count.insert_or_assign(log_message, count + 1);
		std::cout << log_message << "\n";
	}
}

void LogModule::setLogLevel(const LOG_LEVEL& log_level, const LOG_LEVEL_FILTER_TYPE& log_level_filter)
{
	m_log_level = log_level;
	m_log_level_filter = log_level_filter;
}

void LogModule::log(const LOG_LEVEL& log_level, const std::string& source, const std::string& message)
{

	if ((m_log_level_filter == LOG_LEVEL_FILTER_TYPE::EQUAL && log_level == m_log_level) ||
		(m_log_level_filter == LOG_LEVEL_FILTER_TYPE::GREATER_OR_EQUAL && log_level >= m_log_level))
	{
		printLogMessage(m_log_level_mappings.find(log_level)->second, source, message);
	}
}

LogModule* LogModule::getLoggerInstance()
{
	if (logger_instance == nullptr)
	{
		logger_instance = std::unique_ptr<LogModule>(new LogModule());
	}
	return logger_instance.get();
}