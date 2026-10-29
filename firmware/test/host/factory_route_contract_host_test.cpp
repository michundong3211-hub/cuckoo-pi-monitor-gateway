#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

std::string read_file(const char* path) {
  std::ifstream input(path);
  if (!input.good()) {
    std::cerr << "cannot open " << path << "\n";
    ++failures;
    return {};
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string without_whitespace(const std::string& source) {
  std::string compact;
  for (unsigned char character : source) {
    if (!std::isspace(character)) compact.push_back(character);
  }
  return compact;
}

void require_ordered(const std::string& source,
                     const std::vector<std::string>& tokens,
                     const char* contract) {
  size_t offset = 0;
  for (const std::string& token : tokens) {
    const size_t found = source.find(token, offset);
    if (found == std::string::npos) {
      std::cerr << "missing factory route contract: " << contract << " ("
                << token << ")\n";
      ++failures;
      return;
    }
    offset = found + token.size();
  }
}

}  // namespace

int main() {
  const std::string source = read_file("src/web_gateway.cpp");
  const std::string compact = without_whitespace(source);

  require_ordered(
      compact,
      {"boolb=BrewingWebServer::setupWebServer(serviceName);",
       "_server->on(\"/api/factory\",HTTP_POST,[this](AsyncWebServerRequest*request){webHandleFactoryDefaults(request);});"},
      "register bodyless POST after base routes and delegate to the inherited handler");

  return failures == 0 ? 0 : 1;
}
