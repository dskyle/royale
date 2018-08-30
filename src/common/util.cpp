#include <royale/util.hpp>

namespace royale { namespace xtd {

int errchk_throw(const char *fn, const char *file, int line)
{
  int e = errno;
  std::ostringstream s;
  s << fn << ": " << strerror(e);
  s << " [" << file << ":" << line << "]";
  throw std::runtime_error(s.str());
}

std::string file_to_string(const char *filename)
{
  std::ifstream file(filename);
  if (file.is_open()) {
    std::string str;

    file.seekg(0, std::ios::end);
    str.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

    return str;
  } else {
    throw std::runtime_error(std::string("File \"")
        + filename + "\" not found.");
  }
}

} } // namespace royale::xtd
