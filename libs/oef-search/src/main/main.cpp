#include "mt-search/main/src/cpp/MtSearch.hpp"

#include <boost/program_options.hpp>

int main(int argc, char *argv[])
{
  // parse args

  std::string config_file, config_string;

  program_options::options_description desc{"Options"};

  desc.add_options()("config_file",
                     program_options::value<std::string>(&config_file)->default_value(""),
                     "Path to the configuration file.");

  desc.add_options()("config_string",
                     program_options::value<std::string>(&config_string)->default_value(""),
                     "Configuration JSON.");
  program_options::variables_map vm;
  try
  {
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);
  }
  catch (std::exception &ex)
  {
    FETCH_LOG_WARN("MAIN", "Failed to parse command line arguments: ", ex.what());
    return 1;
  }
  // copy from VM to args.

  MtSearch mySearch;

  if (config_file.empty() && config_string.empty())
  {
    FETCH_LOG_WARN("MAIN", "Configuration not provided!");
    desc.print(std::cout);
    return 1;
  }

  if (!mySearch.configure(config_file, config_string))
  {
    FETCH_LOG_WARN("MAIN", "Configuration failed, shutting down...");
    return 1;
  }

  mySearch.run();
}
