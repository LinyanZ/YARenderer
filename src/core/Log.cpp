#include "Log.h"

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Init()
{
    spdlog::set_pattern("%^[%T] %6n: %v%$");

    s_Logger = spdlog::stdout_color_mt("RENDERER");
    s_Logger->set_level(spdlog::level::trace);
}