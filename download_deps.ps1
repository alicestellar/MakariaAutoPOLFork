# Create include directory if it doesn't exist
New-Item -ItemType Directory -Force -Path "include"
New-Item -ItemType Directory -Force -Path "include\nlohmann"

# Download nlohmann/json
Invoke-WebRequest -Uri "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" -OutFile "include\nlohmann\json.hpp"

# Download httplib
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h" -OutFile "include\httplib.h" 