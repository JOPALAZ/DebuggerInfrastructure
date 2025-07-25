#include <stdexcept>
namespace DebuggerInfrastructure
{
    class BadRequestException : public std::runtime_error
    {
        public:
        explicit BadRequestException(std::string msg)
        :std::runtime_error(msg) {}
        explicit BadRequestException(const std::string& msg)
        :std::runtime_error(msg) {}
        explicit BadRequestException(std::string& msg)
        :std::runtime_error(msg) {}
        explicit BadRequestException(const char* msg)
        :std::runtime_error(msg) {}
    };
}