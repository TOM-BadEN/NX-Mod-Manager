/*
    Copyright 2019 natinusala
    Copyright 2019 p-sam

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <fmt/core.h>
#include <stdio.h>

#include <borealis/core/logger.hpp>

namespace brls
{

void Logger::setLogLevel(LogLevel newLogLevel)
{
    Logger::logLevel = newLogLevel;
}

LogLevel Logger::getLogLevel()
{
    return Logger::logLevel;
}

void Logger::setLogOutput(std::FILE *newLogOut)
{
    Logger::logOut = newLogOut;
}

void Logger::setThreadSafeLogging(bool newThreadSafeLogging) {
    Logger::threadSafeLogging = newThreadSafeLogging;
}

} // namespace brls
